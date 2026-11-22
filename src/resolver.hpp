// resolver.hpp — il2cpp 运行时符号解析
// 不写死任何偏移：启动时通过 il2cpp_* 导出 API 直接拿到字段 offset，
// 游戏小版本更新（重新编译 global-metadata）后依然可用。
#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>
#include <dlfcn.h>
#include <android/log.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  "aux", __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  "aux", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "aux", __VA_ARGS__)

using Il2CppClass  = void;
using Il2CppImage  = void;
using Il2CppDomain = void;
using Il2CppObject = void;
using MethodInfo   = void;
using FieldInfo    = void;

class IL2CPP {
public:
    bool ok = false;

    Il2CppDomain* (*domain_get)()                                     = nullptr;
    const void**  (*domain_get_assemblies)(Il2CppDomain*, size_t*)    = nullptr;
    Il2CppImage*  (*assembly_get_image)(const void*)                  = nullptr;
    Il2CppClass*  (*class_from_name)(Il2CppImage*, const char*, const char*) = nullptr;
    const char*   (*class_get_name)(Il2CppClass*)                     = nullptr;
    const char*   (*class_get_namespace)(Il2CppClass*)                = nullptr;
    size_t        (*image_get_class_count)(Il2CppImage*)              = nullptr;
    Il2CppClass*  (*image_get_class)(Il2CppImage*, size_t)            = nullptr;
    FieldInfo*    (*class_get_field_from_name)(Il2CppClass*, const char*) = nullptr;
    size_t        (*field_get_offset)(FieldInfo*)                     = nullptr;
    MethodInfo*   (*class_get_method_from_name)(Il2CppClass*, const char*, int) = nullptr;
    void          (*field_get_value)(Il2CppObject*, FieldInfo*, void*) = nullptr;
    Il2CppObject* (*field_get_value_object)(FieldInfo*, Il2CppObject*)= nullptr;
    void          (*field_static_get_value)(FieldInfo*, void*)        = nullptr;
    void*         (*domain_assembly_open)(Il2CppDomain*, const char*) = nullptr;
    void          (*thread_attach)(Il2CppDomain*)                     = nullptr;
    Il2CppObject* (*object_new)(Il2CppClass*)                         = nullptr;
    int32_t       (*class_instance_size)(Il2CppClass*)                = nullptr;
    const char*   (*image_get_name)(Il2CppImage*)                     = nullptr;

    Il2CppDomain* domain = nullptr;
    uint64_t libBase = 0;

private:
    std::unordered_map<std::string, uint64_t> cache_;
    std::unordered_map<std::string, Il2CppClass*> classes_;
    std::vector<Il2CppImage*> images_;

    template <class T> bool sym(const char* n, T& out) {
        // Zygisk 注入的模块以 RTLD_LOCAL 装载，RTLD_DEFAULT 搜不到 il2cpp 的导出，
        // 必须先拿到 libil2cpp.so 的 handle 再逐个 dlsym。
        static void* hlib = nullptr;
        if (!hlib) {
            hlib = dlopen("libil2cpp.so", RTLD_NOW | RTLD_NOLOAD);
            if (!hlib) hlib = dlopen("libil2cpp.so", RTLD_NOW);
            if (hlib) libBase = (uint64_t)hlib;
        }
        void* p = nullptr;
        if (hlib) p = dlsym(hlib, n);
        if (!p)   p = dlsym(RTLD_DEFAULT, n);
        out = (T)p;
        return p != nullptr;
    }

public:
    bool init() {
        int n = 0;
        n += sym("il2cpp_domain_get",             domain_get);
        n += sym("il2cpp_domain_get_assemblies",  domain_get_assemblies);
        n += sym("il2cpp_assembly_get_image",     assembly_get_image);
        n += sym("il2cpp_class_from_name",        class_from_name);
        n += sym("il2cpp_class_get_name",         class_get_name);
        n += sym("il2cpp_class_get_namespace",    class_get_namespace);
        n += sym("il2cpp_image_get_class_count",  image_get_class_count);
        n += sym("il2cpp_image_get_class",        image_get_class);
        n += sym("il2cpp_class_get_field_from_name", class_get_field_from_name);
        n += sym("il2cpp_field_get_offset",       field_get_offset);
        n += sym("il2cpp_class_get_method_from_name", class_get_method_from_name);
        n += sym("il2cpp_field_get_value",        field_get_value);
        n += sym("il2cpp_field_get_value_object", field_get_value_object);
        n += sym("il2cpp_field_static_get_value", field_static_get_value);
        n += sym("il2cpp_domain_assembly_open",   domain_assembly_open);
        n += sym("il2cpp_thread_attach",          thread_attach);
        n += sym("il2cpp_object_new",             object_new);
        n += sym("il2cpp_class_instance_size",    class_instance_size);
        n += sym("il2cpp_image_get_name",         image_get_name);

        if (!domain_get || !domain_get_assemblies || !assembly_get_image ||
            !class_from_name || !class_get_field_from_name || !field_get_offset) {
            LOGE("il2cpp 关键导出缺失, 已拿 %d/19", n);
            return false;
        }

        domain = domain_get();
        if (!domain) { LOGE("il2cpp domain null"); return false; }
        if (thread_attach) thread_attach(domain);

        size_t cnt = 0;
        const void** asms = domain_get_assemblies(domain, &cnt);
        for (size_t i = 0; i < cnt; ++i) {
            Il2CppImage* im = assembly_get_image(asms[i]);
            if (im) images_.push_back(im);
        }
        LOGI("il2cpp ok: %zu assemblies, symbols %d/19", images_.size(), n);
        ok = true;
        return true;
    }

    // 跨 image 搜类
    Il2CppClass* findClass(const char* name, const char* ns = nullptr) {
        if (!ok) return nullptr;
        std::string key = (ns ? std::string(ns) + "::" : "") + name;
        auto it = classes_.find(key);
        if (it != classes_.end()) return it->second;

        for (Il2CppImage* im : images_) {
            Il2CppClass* k = class_from_name(im, ns ? ns : "", name);
            if (k) { classes_[key] = k; return k; }
        }
        // 全量遍历（处理 namespace 位置不确定的情况）
        for (Il2CppImage* im : images_) {
            size_t c = image_get_class_count ? image_get_class_count(im) : 0;
            for (size_t i = 0; i < c; ++i) {
                Il2CppClass* k = image_get_class(im, i);
                if (!k) continue;
                const char* cn = class_get_name ? class_get_name(k) : nullptr;
                if (cn && strcmp(cn, name) == 0) {
                    classes_[key] = k;
                    return k;
                }
            }
        }
        LOGW("class not found: %s", name);
        return nullptr;
    }

    uint64_t field(const char* cls, const char* fld, const char* ns = nullptr) {
        std::string key = std::string(cls) + "." + fld;
        auto it = cache_.find(key);
        if (it != cache_.end()) return it->second;

        Il2CppClass* k = findClass(cls, ns);
        uint64_t off = 0;
        if (k) {
            FieldInfo* f = class_get_field_from_name(k, fld);
            if (f) off = field_get_offset(f);
            else   LOGW("field %s.%s 缺失", cls, fld);
        }
        cache_[key] = off;
        return off;
    }

    // 静态字段：返回其静态区地址（IsStatic 字段的 offset 就是相对 static data 的偏移）
    void* staticFieldAddr(const char* cls, const char* fld, const char* ns = nullptr) {
        Il2CppClass* k = findClass(cls, ns);
        if (!k || !class_get_field_from_name) return nullptr;
        FieldInfo* f = class_get_field_from_name(k, fld);
        if (!f) return nullptr;
        static uint8_t buf[8];
        // 静态字段的 FieldInfo->offset 指向类静态区，这里通过 field_static_get_value 拿到指针值
        if (field_static_get_value) {
            memset(buf, 0, sizeof buf);
            field_static_get_value(f, buf);
            uint64_t p = *(uint64_t*)buf;
            return (void*)p;
        }
        return nullptr;
    }

    // 按「实例对象的类」查字段偏移（读 world 对象上的字段用）
    uint64_t fieldForInstance(uint64_t obj, const char* fld) {
        if (!obj || !class_get_field_from_name) return 0;
        uint64_t k = *(uint64_t*)obj;              // il2cpp object header = klass*
        if (!k) return 0;
        FieldInfo* f = class_get_field_from_name((Il2CppClass*)k, fld);
        return f ? field_get_offset(f) : 0;
    }

    // 方法 native 指针：MethodInfo 首字段即 methodPointer
    void* methodAddr(const char* cls, const char* name, int argc = 0,
                     const char* ns = nullptr) {
        Il2CppClass* k = findClass(cls, ns);
        if (!k || !class_get_method_from_name) return nullptr;
        MethodInfo* mi = class_get_method_from_name(k, name, argc);
        return mi ? *(void**)mi : nullptr;
    }
};

inline IL2CPP g_il2;
