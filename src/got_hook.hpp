// got_hook.hpp — 全库 GOT/PLT 重定位表 hook（加固版）
// 与原件差异：
//   1. 原件收「第一个命中」就返回 —— 实际进程里 libil2cpp.so / libc.so / 各游戏 so
//      都有自己独立的 GOT 表项，只改一处等于只拦一部分调用方，检测方随便换个
//      调用点就读到真东西。这里遍历全部已加载库，逐个表项都改。
//   2. 同时处理 DT_JMPREL(.rela.plt) 与 DT_RELA(.rela.dyn)，
//      R_AARCH64_JUMP_SLOT 与 R_AARCH64_GLOB_DAT 两种重定位类型。
//   3. 只改 GOT，不碰 .text ⇒ 代码段 hash 校验照过。
//   4. 记录所有被改的页，一次 mprotect 批量改完再统一还原只读（RELRO 外观不变）。
#pragma once
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <link.h>
#include <sys/mman.h>
#include <elf.h>
#include <cstddef>
#include <vector>
#include <set>
#include <mutex>
#include "syscall.hpp"
#include "resolver.hpp"

namespace gh {

struct Scan {
    const char* sym;
    const char* onlyLib;
    std::vector<uintptr_t> hits;
    std::set<uintptr_t>    seen;
};

static inline uintptr_t addrFix(uintptr_t base, uintptr_t p) {
    return (p && p < base) ? base + p : p;
}

static int scanCb(struct dl_phdr_info* info, size_t, void* d) {
    auto* s = (Scan*)d;
    if (!info->dlpi_name || !info->dlpi_name[0]) return 0;
    if (s->onlyLib && !strstr(info->dlpi_name, s->onlyLib)) return 0;
    if (strstr(info->dlpi_name, "libaux")) return 0;      // 不碰自己

    for (int i = 0; i < info->dlpi_phnum; ++i) {
        const ElfW(Phdr)& ph = info->dlpi_phdr[i];
        if (ph.p_type != PT_DYNAMIC) continue;

        auto* dyn = (ElfW(Dyn)*)(info->dlpi_addr + ph.p_vaddr);
        ElfW(Sym)*  symtab = nullptr;
        const char* strtab = nullptr;
        void*       jmprel = nullptr;
        size_t      pltrelsz = 0;
        void*       rela = nullptr;
        size_t      relasz = 0;

        for (auto* p = dyn; p->d_tag != DT_NULL; ++p) {
            switch (p->d_tag) {
            case DT_SYMTAB:   symtab = (ElfW(Sym)*)addrFix(info->dlpi_addr, p->d_un.d_ptr); break;
            case DT_STRTAB:   strtab = (const char*)addrFix(info->dlpi_addr, p->d_un.d_ptr); break;
            case DT_JMPREL:   jmprel = (void*)addrFix(info->dlpi_addr, p->d_un.d_ptr); break;
            case DT_PLTRELSZ: pltrelsz = p->d_un.d_val; break;
            case DT_RELA:     rela = (void*)addrFix(info->dlpi_addr, p->d_un.d_ptr); break;
            case DT_RELASZ:   relasz = p->d_un.d_val; break;
            }
        }
        if (!symtab || !strtab) continue;

        auto sweep = [&](ElfW(Rela)* r, size_t n) {
            if (!r || !n) return;
            size_t cnt = n / sizeof(ElfW(Rela));
            for (size_t k = 0; k < cnt; ++k) {
                unsigned type = ELF64_R_TYPE(r[k].r_info);
                if (type != R_AARCH64_JUMP_SLOT && type != R_AARCH64_GLOB_DAT) continue;
                unsigned si = ELF64_R_SYM(r[k].r_info);
                if (!si) continue;
                const char* nm = strtab + symtab[si].st_name;
                if (!nm || strcmp(nm, s->sym) != 0) continue;
                uintptr_t got = info->dlpi_addr + r[k].r_offset;
                if (s->seen.insert(got).second) s->hits.push_back(got);
            }
        };

        sweep((ElfW(Rela)*)jmprel, pltrelsz);
        sweep((ElfW(Rela)*)rela,   relasz);
    }
    return 0;
}

// 找出符号在所有已加载库中的全部 GOT 表项
static inline std::vector<uintptr_t> findAll(const char* sym, const char* onlyLib = nullptr) {
    Scan s{ sym, onlyLib, {}, {} };
    dl_iterate_phdr(scanCb, &s);
    return s.hits;
}

// 批量改：一次算好所有页，一起放开写权限，写完一起还原只读
static inline bool patchMany(const std::vector<uintptr_t>& slots, void* fn) {
    if (slots.empty()) return false;
    const long PG = 4096;
    std::set<uintptr_t> pages;
    for (auto g : slots) pages.insert(g & ~(uintptr_t)(PG - 1));

    for (auto pg : pages)
        if (sc::sys_mprotect((void*)pg, PG, PROT_READ | PROT_WRITE) != 0) return false;

    for (auto g : slots) *(void**)g = fn;
    __builtin___clear_cache((char*)*slots.begin(), (char*)*slots.begin() + 8);

    for (auto pg : pages)
        sc::sys_mprotect((void*)pg, PG, PROT_READ);

    return true;
}

static inline bool patch(uintptr_t got, void* fn) {
    if (!got) return false;
    std::vector<uintptr_t> v{ got };
    return patchMany(v, fn);
}

// 返回实际改成功的表项数；oldFn 取第一个原始值
static inline int hook(const char* sym, void* newFn, void** oldFn, const char* lib = nullptr) {
    auto slots = findAll(sym, lib);
    if (slots.empty()) return 0;
    if (oldFn) *oldFn = *(void**)slots[0];
    return patchMany(slots, newFn) ? (int)slots.size() : 0;
}

} // namespace gh
