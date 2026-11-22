// loader.cpp — 载荷伪装 / 字符串混淆 / 痕迹清理
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "resolver.hpp"

namespace loader {

// ---------------- 自路径 ----------------
std::string selfPath(const char* key) {
    FILE* f = fopen("/proc/self/maps", "r");
    if (!f) return {};
    char line[512];
    std::string r;
    while (fgets(line, sizeof line, f)) {
        if (strstr(line, key)) {
            char* p = strchr(line, '/');
            if (p) {
                char* e = strchr(p, '\n');
                if (e) *e = 0;
                r = p;
                break;
            }
        }
    }
    fclose(f);
    return r;
}

// ---------------- memfd 二次加载 ----------------
// 把磁盘上的 so 读进匿名 memfd，用系统库名字伪装后再 dlopen。
// maps 里看到的就是 "memfd:libGLESv2_adreno.so (deleted)"。
void* loadDisguised(const char* path, const char* fakeName) {
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return nullptr;
    off_t sz = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    if (sz <= 0 || sz > 64 * 1024 * 1024) { close(fd); return nullptr; }

    int mfd = (int)syscall(279, fakeName, 0);   // memfd_create
    if (mfd < 0) { close(fd); return nullptr; }

    char buf[65536];
    ssize_t n;
    while ((n = read(fd, buf, sizeof buf)) > 0) {
        if (write(mfd, buf, (size_t)n) != n) break;
    }
    close(fd);
    lseek(mfd, 0, SEEK_SET);

    char p[64];
    snprintf(p, sizeof p, "/proc/self/fd/%d", mfd);
    void* h = dlopen(p, RTLD_NOW);
    close(mfd);
    if (!h) LOGE("loadDisguised 失败: %s", dlerror());
    return h;
}

// ---------------- 磁盘痕迹 ----------------
void scrubModuleFiles(const char* dir) {
    // 只在内存里已拉起后调用：把落地的 so 删掉，磁盘扫描看不到载荷
    char cmd[256];
    snprintf(cmd, sizeof cmd, "rm -f %s/zygisk/*.so 2>/dev/null", dir);
    if (system(cmd) != 0) { /* 忽略 */ }
}

// ---------------- 运行期自检 ----------------
void selfCheck() {
    std::string p = selfPath("libaux");
    if (!p.empty()) LOGI("self mapped at %s", p.c_str());
    else            LOGI("self path hidden (already filtered)");
}

} // namespace loader
