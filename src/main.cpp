// main.cpp — Zygisk 入口 + eglSwapBuffers 挂载 + ImGui GLES3 渲染
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <pthread.h>
#include <dlfcn.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include "zygisk.hpp"
#include "imgui.h"
#include "imgui_impl_opengl3.h"

#include "draw.hpp"
#include "game.hpp"
#include "config.hpp"
#include "stealth.hpp"
#include "touch.hpp"
#include "input.hpp"
#include "recoil.hpp"
#include "resolver.hpp"
#include "got_hook.hpp"

extern Config g_cfg;

static EGLDisplay g_dpy  = EGL_NO_DISPLAY;
static EGLSurface g_surf = EGL_NO_SURFACE;
static bool g_ready = false;
static bool g_hooked = false;

static double nowSec() {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + ts.tv_nsec / 1e9;
}

void AuxSaveConfig() {
    g_cfg.save("/data/local/tmp/aux.ini");
    LOGI("config saved");
}

// ---------------- ImGui 初始化 ----------------
static void initFonts() {
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;

    const char* paths[] = {
        "/system/fonts/NotoSansCJK-Regular.ttc",
        "/system/fonts/NotoSansSC-Regular.otf",
        "/system/fonts/DroidSansFallback.ttf",
        "/system/fonts/DroidSansFallbackFull.ttf",
        "/system/fonts/MiSans-Regular.ttf",
        "/system/fonts/HONORSansCN-Regular.ttf",
        nullptr
    };
    ImFontConfig fc;
    fc.OversampleH = 2;
    fc.OversampleV = 2;
    fc.PixelSnapH  = true;
    fc.FontNo      = 0;

    ImFont* got = nullptr;
    for (int i = 0; paths[i]; ++i) {
        FILE* f = fopen(paths[i], "rb");
        if (!f) continue;
        fclose(f);
        got = io.Fonts->AddFontFromFileTTF(paths[i], 28.f, &fc,
                io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
        if (got) { LOGI("font: %s", paths[i]); break; }
    }
    if (!got) {
        ImFontConfig d;
        d.SizePixels = 26.f;
        io.Fonts->AddFontDefault(&d);
        LOGW("中文字体缺失, 回退默认字体");
    }
    io.Fonts->Build();
}

int AuxRenderInit(void* d, void* s) {
    g_dpy  = (EGLDisplay)d;
    g_surf = (EGLSurface)s;

    EGLint w = 0, h = 0;
    if (g_dpy != EGL_NO_DISPLAY && g_surf != EGL_NO_SURFACE) {
        eglQuerySurface(g_dpy, g_surf, EGL_WIDTH,  &w);
        eglQuerySurface(g_dpy, g_surf, EGL_HEIGHT, &h);
    }
    if (w > 0 && h > 0) { g_scr.w = w; g_scr.h = h; }
    LOGI("render init %dx%d", g_scr.w, g_scr.h);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    initFonts();

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)g_scr.w, (float)g_scr.h);
    io.DisplayFramebufferScale = ImVec2(1.f, 1.f);

    ImGui_ImplOpenGL3_Init("#version 300 es");

    ImGui::StyleColorsDark();
    ImGuiStyle& st = ImGui::GetStyle();
    st.WindowRounding = 8.f;
    st.FrameRounding  = 6.f;
    st.GrabRounding   = 6.f;
    st.WindowBorderSize = 1.f;
    st.Colors[ImGuiCol_Border] = ImVec4(0.16f, 0.9f, 0.35f, 0.55f);

    g_ready = true;
    return 0;
}

// ---------------- 每帧渲染 ----------------
void AuxRenderFrame() {
    if (!g_ready) return;
    if (g_game.busy.load(std::memory_order_relaxed)) return;

    static int  holdFingers = 0;
    static bool menuState = false;

    TouchReader::Point pt = g_touchReader.poll();

    // 三指同时按住 0.35s 切换菜单
    if (pt.fingers >= 3) {
        if (holdFingers < 30) ++holdFingers;
        if (holdFingers == 22) {
            menuState = !menuState;
            g_cfg.menu = menuState;
        }
    } else {
        holdFingers = 0;
    }

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)g_scr.w, (float)g_scr.h);

    float scaleX = g_touchReader.screenW() > 0
                 ? (float)g_scr.w / (float)g_touchReader.screenW() : 1.f;
    float scaleY = g_touchReader.screenH() > 0
                 ? (float)g_scr.h / (float)g_touchReader.screenH() : 1.f;

    io.MousePos = ImVec2(pt.x * scaleX, pt.y * scaleY);
    io.MouseDown[0] = pt.down;

    // 全屏透明 pass，只画 ESP
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("##root", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
                 ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::End();
    ImGui::PopStyleVar();

    RenderESP();
    DrawMenu();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // 复位 GL 状态，别污染游戏自己的渲染
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glActiveTexture(GL_TEXTURE0);
}

// ---------------- eglSwapBuffers hook ----------------
typedef EGLBoolean (*SwapFn)(EGLDisplay, EGLSurface);
static SwapFn o_swap = nullptr;

static EGLBoolean hk_swap(EGLDisplay d, EGLSurface s) {
    static bool inited = false;
    // 后台正在重建 players 时整帧跳过，避免渲染线程读到半成品（use-after-free）
    if (!g_game.busy.load(std::memory_order_relaxed)) {
        if (!inited) {
            inited = true;
            AuxRenderInit((void*)d, (void*)s);
            while (glGetError() != GL_NO_ERROR) {}   // 清掉初始化期 GL 错误
        }
        AuxRenderFrame();
    }
    return o_swap ? o_swap(d, s) : EGL_TRUE;
}

// ---------------- 工作线程 ----------------
static void* worker(void*) {
    LOGI("aux worker start");

    // 等引擎起来
    for (int i = 0; i < 300; ++i) {
        if (dlopen("libil2cpp.so", RTLD_NOLOAD)) break;
        usleep(100000);
    }
    usleep(500000);

    // 1. 反检测先装上（越早越好）
    stealth::install();

    // 2. 配置
    g_cfg.load("/data/local/tmp/aux.ini");

    // 3. 游戏对象层
    if (!g_game.init()) {
        LOGE("game init 失败, 重试中");
        for (int i = 0; i < 20 && !g_game.schema.ok; ++i) {
            usleep(500000);
            g_game.init();
        }
    }

    // 4. 触摸
    g_touchReader.start();
    int sw = g_touchReader.screenW();
    int sh = g_touchReader.screenH();
    g_touch.init(sw, sh);

    // 5. GL hook
    for (int i = 0; i < 300; ++i) {
        if (dlopen("libEGL.so", RTLD_NOLOAD)) break;
        usleep(100000);
    }
    usleep(300000);

    if (gh::hook("eglSwapBuffers", (void*)hk_swap, (void**)&o_swap)) {
        g_hooked = true;
        LOGI("eglSwapBuffers hooked, old=%p", o_swap);
    } else {
        LOGE("eglSwapBuffers hook 失败");
    }

    // 6. 主循环：数据更新 + 压枪
    double last = nowSec();
    int    skip = 0;
    while (true) {
        usleep(8000);
        double t = nowSec();
        float dt = (float)(t - last);
        last = t;
        if (dt > 0.25f) dt = 0.25f;

        // 游戏数据 30Hz 更新就够，省 CPU
        if (++skip >= 4) {
            skip = 0;
            g_game.update();
        }
        g_recoil.tick(dt);
        stealth::tick();
    }
    return nullptr;
}

// ---------------- Zygisk 模块 ----------------
using namespace zygisk;

class AuxModule : public ModuleBase {
public:
    void onLoad(Api* api, JNIEnv* env) override {
        this->api_ = api;
        this->env_ = env;
    }

    void preAppSpecialize(AppSpecializeArgs* args) override {
        const char* pkg = nullptr;
        if (args->nice_name) pkg = env_->GetStringUTFChars(args->nice_name, nullptr);
        bool target = pkg && strstr(pkg, "com.tencent.mf.uam");
        if (pkg) env_->ReleaseStringUTFChars(args->nice_name, pkg);

        if (!target) {
            // 非目标进程：立刻卸载模块，别留下任何痕迹
            api_->setOption(DLCLOSE_MODULE_LIBRARY);
        }
    }

    void postAppSpecialize(const AppSpecializeArgs*) override {
        pthread_t t;
        if (pthread_create(&t, nullptr, worker, nullptr) == 0)
            pthread_detach(t);
    }

private:
    Api* api_ = nullptr;
    JNIEnv* env_ = nullptr;
};

REGISTER_ZYGISK_MODULE(AuxModule)
