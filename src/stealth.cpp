// stealth.cpp — 反检测层（加固版）
// 相对原件的改动，逐条对应「为什么危险」：
//  1) classify() 改纯 C 指针扫描：原件每个 open() 都构造 std::string 再比较，
//     在游戏主线程上每秒几千次，帧时间抖动本身就是可被检测的信号。
//  2) 新增 denyPath()：原件只处理了 /proc 下的读，/data/adb、aux.ini、模块目录
//     这些「落地痕迹」完全没管 —— 检测方 access()/stat() 一下就知道你在。
//  3) 新增 fstat 一致性：原件返回的 fake fd 是 memfd，st_size = 净化后长度、
//     st_dev/st_ino 都不是 procfs 的。检测方只要 fstat(fd).st_size != 0
//     就能断定「这个 /proc 文件是假的」。这里把 stat 也补齐。
//  4) 特征表去掉 "inject" / "tester" / "aux." 这类过宽词 —— 原件会误删游戏自己的
//     正常映射行，导致 maps 里出现地址空洞，反而更容易被抓。
//  5) opendir 对隐藏路径返回 ENOENT，避免 readdir 暴露模块目录。
#include "stealth.hpp"
#include "got_hook.hpp"
#include "syscall.hpp"
#include "resolver.hpp"

#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <algorithm>
#include <mutex>
#include <unordered_map>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctime>
#include <cctype>
#include <pthread.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <errno.h>

#ifndef O_TMPFILE
#define O_TMPFILE 0
#endif

namespace stealth {

// ---------------- 特征表（精确优先） ----------------
static const char* kHidden[] = {
    "libaux", "anjian", "aux.ini",
    "/data/adb/modules", "/data/adb/ksu", "/data/adb/ap",
    "magisk", "zygisk", "riru", "shamiko", "kernelsu", "apatch", "ksud",
    "frida", "gum-js", "gadget", "linjector", "gmain", "gdbus",
    "xposed", "lspd", "lsposed", "edxposed", "substrate",
    "dobby", "sandhook", "shadowhook", "whale", "bhook",
    nullptr
};

// 存在性探测需要直接判死的路径
static const char* kDeny[] = {
    "/data/adb", "/data/adb/modules", "anjian_esp", "anjian-esp",
    "aux.ini", "libaux", "/sdcard/anjian", "/storage/emulated/0/anjian",
    nullptr
};

static std::mutex g_mx;
static std::unordered_map<int, std::string> g_fdpath;

static inline bool hasNoCase(const char* s, const char* k) {
    if (!s || !k || !*k) return false;
    for (; *s; ++s) {
        const char* a = s; const char* b = k;
        while (*a && *b && tolower((unsigned char)*a) == *b) { ++a; ++b; }
        if (!*b) return true;
    }
    return false;
}

static inline bool hitsFilter(const char* line) {
    for (int i = 0; kHidden[i]; ++i) if (hasNoCase(line, kHidden[i])) return true;
    return false;
}

static inline bool denyPath(const char* p) {
    if (!p) return false;
    for (int i = 0; kDeny[i]; ++i) if (hasNoCase(p, kDeny[i])) return true;
    return false;
}

// 0=不管 1=maps 2=status 3=net 4=mounts
static inline int classify(const char* p) {
    if (!p || (p[0] != '/' )) return 0;
    bool proc = strstr(p, "/proc/") != nullptr;
    bool sys  = strstr(p, "/sys/")  != nullptr;
    if (!proc && !sys) return 0;

    size_t n = strlen(p);
    auto ends = [&](const char* s) {
        size_t l = strlen(s);
        return n >= l && strcmp(p + n - l, s) == 0;
    };
    if (ends("/maps") || ends("/smaps") || ends("/smaps_rollup") || ends("/numa_maps")) return 1;
    if (ends("/status") || ends("/stat")) return 2;
    if (ends("/net/tcp") || ends("/net/tcp6") || ends("/net/udp") || ends("/net/udp6")) return 3;
    if (ends("/mounts") || ends("/mountinfo")) return 4;
    return 0;
}

// ---------------- 内容净化 ----------------
static std::string cleanLines(const std::string& in) {
    std::string out; out.reserve(in.size());
    size_t p = 0;
    while (p < in.size()) {
        size_t e = in.find('\n', p);
        if (e == std::string::npos) e = in.size();
        if (!hitsFilter(in.c_str() + p)) { out.append(in, p, e - p); out += '\n'; }
        p = e + 1;
    }
    return out;
}

static std::string cleanStatus(const std::string& in) {
    std::string out; out.reserve(in.size());
    size_t p = 0;
    while (p < in.size()) {
        size_t e = in.find('\n', p);
        if (e == std::string::npos) e = in.size();
        const char* l = in.c_str() + p;
        if      (!strncmp(l, "TracerPid:", 10)) out += "TracerPid:\t0\n";
        else if (!strncmp(l, "Seccomp:",    8)) out += "Seccomp:\t0\n";
        else if (!strncmp(l, "NoNewPrivs:", 11)) out += "NoNewPrivs:\t0\n";
        else if (!hitsFilter(l)) out.append(in, p, e - p), out += '\n';
        p = e + 1;
    }
    return out;
}

static std::string cleanNet(const std::string& in) {
    std::string out; out.reserve(in.size());
    size_t p = 0;
    while (p < in.size()) {
        size_t e = in.find('\n', p);
        if (e == std::string::npos) e = in.size();
        const char* l = in.c_str() + p;
        if (!hasNoCase(l, "69A2") && !hasNoCase(l, "69A3")) out.append(in, p, e - p), out += '\n';
        p = e + 1;
    }
    return out;
}

// ---------------- fake fd ----------------
static std::string slurp(int fd) {
    std::string s; char buf[8192]; ssize_t n;
    sc::sys_lseek(fd, 0, SEEK_SET);
    while ((n = sc::sys_read(fd, buf, sizeof buf)) > 0) s.append(buf, (size_t)n);
    return s;
}

static int makeFakeFd(const char* orig, const std::string& content) {
    int fd = sc::sys_memfd("aux_anon", 0);
    if (fd < 0) return -1;
    size_t off = 0;
    while (off < content.size()) {
        long n = sc::sys_write(fd, content.data() + off, content.size() - off);
        if (n <= 0) break;
        off += (size_t)n;
    }
    sc::sys_lseek(fd, 0, SEEK_SET);
    std::lock_guard<std::mutex> lk(g_mx);
    g_fdpath[fd] = orig;
    return fd;
}

static int tryFake(const char* path, int flags) {
    int k = classify(path);
    if (!k) return -1;
    if (flags & (O_WRONLY | O_RDWR | O_DIRECTORY | O_PATH)) return -1;

    int rfd = sc::sys_openat(AT_FDCWD, path, O_RDONLY | O_CLOEXEC, 0);
    if (rfd < 0) return -1;
    std::string raw = slurp(rfd);
    sc::sys_close(rfd);

    std::string clean;
    switch (k) {
    case 1: clean = cleanLines(raw);  break;
    case 2: clean = cleanStatus(raw); break;
    case 3: clean = cleanNet(raw);    break;
    case 4: clean = cleanLines(raw);  break;
    default: return -1;
    }
    return makeFakeFd(path, clean);
}

static bool isFakeFd(int fd) {
    std::lock_guard<std::mutex> lk(g_mx);
    return g_fdpath.find(fd) != g_fdpath.end();
}

// ---------------- hooks ----------------
static int     (*o_open)(const char*, int, ...)                    = nullptr;
static int     (*o_open64)(const char*, int, ...)                  = nullptr;
static int     (*o_openat)(int, const char*, int, ...)             = nullptr;
static int     (*o_openat64)(int, const char*, int, ...)           = nullptr;
static FILE*   (*o_fopen)(const char*, const char*)                = nullptr;
static FILE*   (*o_fopen64)(const char*, const char*)              = nullptr;
static ssize_t (*o_readlink)(const char*, char*, size_t)           = nullptr;
static ssize_t (*o_readlinkat)(int, const char*, char*, size_t)    = nullptr;
static int     (*o_access)(const char*, int)                       = nullptr;
static int     (*o_faccessat)(int, const char*, int, int)          = nullptr;
static int     (*o_stat)(const char*, struct stat*)                 = nullptr;
static int     (*o_lstat)(const char*, struct stat*)                = nullptr;
static int     (*o_fstatat)(int, const char*, struct stat*, int)    = nullptr;
static int     (*o_fstat)(int, struct stat*)                       = nullptr;
static DIR*    (*o_opendir)(const char*)                           = nullptr;

static int h_open(const char* p, int fl, ...) {
    mode_t m = 0;
    if (fl & (O_CREAT | O_TMPFILE)) { va_list ap; va_start(ap, fl); m = va_arg(ap, int); va_end(ap); }
    if (denyPath(p)) { errno = ENOENT; return -1; }
    int f = tryFake(p, fl);
    if (f >= 0) return f;
    return o_open ? o_open(p, fl, m) : sc::sys_openat(AT_FDCWD, p, fl, m);
}

static int h_open64(const char* p, int fl, ...) {
    mode_t m = 0;
    if (fl & (O_CREAT | O_TMPFILE)) { va_list ap; va_start(ap, fl); m = va_arg(ap, int); va_end(ap); }
    if (denyPath(p)) { errno = ENOENT; return -1; }
    int f = tryFake(p, fl);
    if (f >= 0) return f;
    return o_open64 ? o_open64(p, fl, m) : sc::sys_openat(AT_FDCWD, p, fl, m);
}

static int h_openat(int dfd, const char* p, int fl, ...) {
    mode_t m = 0;
    if (fl & (O_CREAT | O_TMPFILE)) { va_list ap; va_start(ap, fl); m = va_arg(ap, int); va_end(ap); }
    if (denyPath(p)) { errno = ENOENT; return -1; }
    if (dfd == AT_FDCWD) { int f = tryFake(p, fl); if (f >= 0) return f; }
    return o_openat ? o_openat(dfd, p, fl, m) : sc::sys_openat(dfd, p, fl, m);
}

static int h_openat64(int dfd, const char* p, int fl, ...) {
    mode_t m = 0;
    if (fl & (O_CREAT | O_TMPFILE)) { va_list ap; va_start(ap, fl); m = va_arg(ap, int); va_end(ap); }
    if (denyPath(p)) { errno = ENOENT; return -1; }
    if (dfd == AT_FDCWD) { int f = tryFake(p, fl); if (f >= 0) return f; }
    return o_openat64 ? o_openat64(dfd, p, fl, m) : sc::sys_openat(dfd, p, fl, m);
}

static FILE* fakeFopen(const char* p, const char* mode) {
    if (!mode) return nullptr;
    if (strpbrk(mode, "wa+")) return nullptr;
    int f = tryFake(p, O_RDONLY);
    if (f < 0) return nullptr;
    return fdopen(f, mode);
}

static FILE* h_fopen(const char* p, const char* m) {
    if (denyPath(p)) { errno = ENOENT; return nullptr; }
    FILE* r = fakeFopen(p, m);
    if (r) return r;
    return o_fopen ? o_fopen(p, m) : nullptr;
}

static FILE* h_fopen64(const char* p, const char* m) {
    if (denyPath(p)) { errno = ENOENT; return nullptr; }
    FILE* r = fakeFopen(p, m);
    if (r) return r;
    return o_fopen64 ? o_fopen64(p, m) : nullptr;
}

static int h_access(const char* p, int mode) {
    if (denyPath(p)) { errno = ENOENT; return -1; }
    return o_access ? o_access(p, mode) : -1;
}
static int h_faccessat(int d, const char* p, int mode, int fl) {
    if (denyPath(p)) { errno = ENOENT; return -1; }
    return o_faccessat ? o_faccessat(d, p, mode, fl) : -1;
}
static int h_stat(const char* p, struct stat* st) {
    if (denyPath(p)) { errno = ENOENT; return -1; }
    return o_stat ? o_stat(p, st) : -1;
}
static int h_lstat(const char* p, struct stat* st) {
    if (denyPath(p)) { errno = ENOENT; return -1; }
    return o_lstat ? o_lstat(p, st) : -1;
}
static int h_fstatat(int d, const char* p, struct stat* st, int fl) {
    if (denyPath(p)) { errno = ENOENT; return -1; }
    return o_fstatat ? o_fstatat(d, p, st, fl) : -1;
}

// fake fd 的 stat 要和「真的 procfs 文件」一致：size=0、dev/ino 稳定、0644
static int h_fstat(int fd, struct stat* st) {
    if (isFakeFd(fd)) {
        memset(st, 0, sizeof(struct stat));
        st->st_dev     = 0x1f;                 // procfs 的 st_dev
        st->st_ino     = 0xC0FFEE00u + (unsigned)fd;
        st->st_mode    = S_IFREG | 0444;
        st->st_nlink   = 1;
        st->st_size    = 0;                    // 关键：真 /proc 文件 size 是 0
        st->st_blksize = 4096;
        st->st_blocks  = 0;
        return 0;
    }
    return o_fstat ? o_fstat(fd, st) : -1;
}

static DIR* h_opendir(const char* p) {
    if (denyPath(p)) { errno = ENOENT; return nullptr; }
    return o_opendir ? o_opendir(p) : nullptr;
}

static bool fdPathOf(const char* p, std::string& orig) {
    int fd = -1;
    if (sscanf(p, "/proc/self/fd/%d", &fd) != 1 &&
        sscanf(p, "/proc/%*d/fd/%d", &fd) != 1) return false;
    if (fd < 0) return false;
    std::lock_guard<std::mutex> lk(g_mx);
    auto it = g_fdpath.find(fd);
    if (it == g_fdpath.end()) return false;
    orig = it->second;
    return true;
}

static ssize_t writeRet(char* buf, size_t sz, const std::string& s) {
    if (!buf || sz == 0) return 0;
    size_t n = std::min(sz - 1, s.size());
    memcpy(buf, s.data(), n);
    buf[n] = 0;
    return (ssize_t)n;
}

static ssize_t h_readlink(const char* p, char* b, size_t s) {
    std::string orig;
    if (fdPathOf(p, orig)) return writeRet(b, s, orig);
    return o_readlink ? o_readlink(p, b, s) : -1;
}
static ssize_t h_readlinkat(int d, const char* p, char* b, size_t s) {
    std::string orig;
    if (fdPathOf(p, orig)) return writeRet(b, s, orig);
    return o_readlinkat ? o_readlinkat(d, p, b, s) : -1;
}

// ---------------- 分析器清理 ----------------
void killAnalyzers() {
    DIR* d = opendir("/proc");
    if (!d) return;
    const char* bad[] = { "frida-server", "frida-helper", "gdbserver",
                          "linjector", "gdb --", nullptr };
    dirent* e;
    while ((e = readdir(d))) {
        if (e->d_name[0] < '0' || e->d_name[0] > '9') continue;
        char p[64];
        snprintf(p, sizeof p, "/proc/%s/cmdline", e->d_name);
        int fd = sc::sys_openat(AT_FDCWD, p, O_RDONLY | O_CLOEXEC, 0);
        if (fd < 0) continue;
        char buf[256] = {0};
        sc::sys_read(fd, buf, sizeof buf - 1);
        sc::sys_close(fd);
        if (!buf[0]) continue;
        for (int i = 0; bad[i]; ++i) {
            if (strstr(buf, bad[i])) {
                int pid = atoi(e->d_name);
                if (pid > 0 && pid != getpid()) { kill(pid, SIGKILL); LOGW("kill %s/%d", bad[i], pid); }
                break;
            }
        }
    }
    closedir(d);
}

static void hideThreadNames() {
    DIR* d = opendir("/proc/self/task");
    if (!d) return;
    dirent* e;
    while ((e = readdir(d))) {
        if (e->d_name[0] < '0' || e->d_name[0] > '9') continue;
        char p[96];
        snprintf(p, sizeof p, "/proc/self/task/%s/comm", e->d_name);
        int fd = sc::sys_openat(AT_FDCWD, p, O_RDONLY | O_CLOEXEC, 0);
        if (fd < 0) continue;
        char name[64] = {0};
        sc::sys_read(fd, name, sizeof name - 1);
        sc::sys_close(fd);
        if (strstr(name, "frida") || strstr(name, "gum-") ||
            strstr(name, "gmain") || strstr(name, "gdbus") || strstr(name, "aux")) {
            int fdw = sc::sys_openat(AT_FDCWD, p, O_WRONLY | O_CLOEXEC, 0);
            if (fdw >= 0) { const char* nn = "Binder:worker\n"; sc::sys_write(fdw, nn, strlen(nn)); sc::sys_close(fdw); }
        }
    }
    closedir(d);
}

// ---------------- 入口 ----------------
static bool installed = false;

void install() {
    if (installed) return;
    installed = true;

    struct Item { const char* name; void* fn; void** old; };
    Item items[] = {
        { "open",       (void*)h_open,      (void**)&o_open      },
        { "open64",     (void*)h_open64,    (void**)&o_open64    },
        { "openat",     (void*)h_openat,    (void**)&o_openat    },
        { "openat64",   (void*)h_openat64,  (void**)&o_openat64  },
        { "fopen",      (void*)h_fopen,     (void**)&o_fopen     },
        { "fopen64",    (void*)h_fopen64,   (void**)&o_fopen64   },
        { "readlink",   (void*)h_readlink,  (void**)&o_readlink  },
        { "readlinkat", (void*)h_readlinkat,(void**)&o_readlinkat},
        { "access",     (void*)h_access,    (void**)&o_access    },
        { "faccessat",  (void*)h_faccessat, (void**)&o_faccessat },
        { "stat",       (void*)h_stat,      (void**)&o_stat      },
        { "lstat",      (void*)h_lstat,     (void**)&o_lstat     },
        { "fstatat",    (void*)h_fstatat,   (void**)&o_fstatat   },
        { "fstat",      (void*)h_fstat,     (void**)&o_fstat     },
        { "opendir",    (void*)h_opendir,   (void**)&o_opendir   },
    };

    int slots = 0;
    for (auto& it : items) {
        int n = gh::hook(it.name, it.fn, it.old);
        if (n) ++slots;
        LOGI("stealth: %-10s -> %d GOT slots", it.name, n);
    }
    LOGI("stealth: %d/%zu symbols, %d slots total", slots,
         sizeof(items) / sizeof(items[0]), slots);

    killAnalyzers();
    hideThreadNames();
    sc::raw(sc::SYS_prctl, 4 /*PR_SET_DUMPABLE*/, 0);
}

void tick() {
    static uint64_t last = 0;
    uint64_t now = (uint64_t)time(nullptr);
    if (now - last < 5) return;
    last = now;
    hideThreadNames();
}

} // namespace stealth
