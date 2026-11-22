// memory.hpp — 进程内存读取（同进程直读，无需 /proc/pid/mem）
#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <cstdio>
#include "resolver.hpp"

class Mem {
public:
    uint64_t moduleBase(const char* so) {
        FILE* f = fopen("/proc/self/maps", "r");
        if (!f) return 0;
        char line[512]; uint64_t base = 0;
        while (fgets(line, sizeof line, f)) {
            if (strstr(line, so)) {
                base = strtoull(line, nullptr, 16);
                break;
            }
        }
        fclose(f);
        return base;
    }

    inline bool valid(uint64_t a) { return a > 0x10000 && a < 0x7fffffffffffULL; }

    template <class T>
    T rd(uint64_t addr, T def = T{}) {
        if (!valid(addr)) return def;
        T v{};
        memcpy(&v, (void*)addr, sizeof(T));
        return v;
    }
    uint64_t ptr(uint64_t a) { return rd<uint64_t>(a, 0); }
    float    f32(uint64_t a) { return rd<float>(a, 0.f); }
    int32_t  i32(uint64_t a) { return rd<int32_t>(a, 0); }

    // il2cpp string (UTF-16) -> utf8
    std::string str(uint64_t addr) {
        if (!valid(addr)) return {};
        int32_t len = rd<int32_t>(addr + 0x10, 0);
        if (len <= 0 || len > 64) return {};
        std::string o; o.resize(len);
        const uint16_t* w = (const uint16_t*)(addr + 0x14);
        for (int i = 0; i < len; ++i) o[i] = (w[i] < 0x80) ? (char)w[i] : '?';
        return o;
    }

    // List<T> 遍历
    template <class F>
    void listEach(uint64_t list, F&& fn) {
        if (!valid(list)) return;
        uint64_t items = ptr(list + 0x10);
        int32_t  size  = i32(list + 0x18);
        if (!valid(items) || size <= 0 || size > 1024) return;
        for (int i = 0; i < size; ++i) {
            uint64_t o = ptr(items + 8ull * i);
            if (valid(o)) fn(o);
        }
    }

    // C# Dictionary / HashSet 的 entries 遍历（简化版，只走 buckets 链表）
    template <class F>
    void dictEach(uint64_t dict, F&& fn) {
        if (!valid(dict)) return;
        uint64_t entries = ptr(dict + 0x18);
        int32_t  count   = i32(dict + 0x20);
        if (!valid(entries) || count <= 0 || count > 4096) return;
        for (int i = 0; i < count; ++i) {
            uint64_t e = entries + (uint64_t)i * 24;
            uint64_t key = ptr(e + 0x8);
            uint64_t val = ptr(e + 0x10);
            if (valid(key)) fn(key, val);
        }
    }
};

inline Mem g_mem;
