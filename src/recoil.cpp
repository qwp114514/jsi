// recoil.cpp — 触摸压枪 + 视角辅助
// 走 uinput 独立 pointer，不写游戏内存，不碰 .text，
// 游戏侧看到的只是"玩家右拇指在往下拖"，这就是它应有的样子。
#include "recoil.hpp"
#include "config.hpp"
#include "game.hpp"
#include "touch.hpp"
#include "resolver.hpp"
#include <cmath>
#include <cstdlib>

Recoil g_recoil;
extern Config g_cfg;

// 武器后坐力表：垂直速度(px/s @1080p) / 水平抖动
struct WeaponProf { const char* key; float v; float h; };
static const WeaponProf kProf[] = {
    { "AK",      260.f, 45.f },
    { "M4",      205.f, 35.f },
    { "M16",     230.f, 30.f },
    { "SCAR",    240.f, 40.f },
    { "HK416",   200.f, 32.f },
    { "MP5",     150.f, 25.f },
    { "UMP",     165.f, 28.f },
    { "Vector",  140.f, 22.f },
    { "M870",    380.f, 20.f },
    { "S12K",    360.f, 25.f },
    { "SVD",     300.f, 18.f },
    { "M700",    420.f, 10.f },
    { "PKM",     280.f, 50.f },
    { "RPK",     250.f, 45.f },
    { nullptr,   220.f, 35.f },
};

static const WeaponProf& prof(float scale) {
    (void)scale;
    return kProf[sizeof(kProf) / sizeof(kProf[0]) - 1];   // 默认值
}

void Recoil::start() {
    offX_ = offY_ = curX_ = curY_ = 0;
    active_ = false;
    idle_ = 0;
    if (!g_touch.ok()) return;

    // 虚拟手指落在右侧视角区
    float bx = g_scr.w * 0.72f;
    float by = g_scr.h * 0.58f;
    g_touch.down(slot_, (int32_t)bx, (int32_t)by);
}

void Recoil::stop() {
    if (active_ && g_touch.ok()) g_touch.up(slot_);
    active_ = false;
    offX_ = offY_ = curX_ = curY_ = 0;
}

void Recoil::tick(float dt) {
    if (!g_cfg.recoil && !g_cfg.aimAssist) return;
    if (!g_touch.ok()) return;

    const float bx = g_scr.w * 0.72f;
    const float by = g_scr.h * 0.58f;

    bool firing = g_game.firing && g_game.inRaid;

    // ---------- 压枪 ----------
    float targetX = 0, targetY = 0;

    if (g_cfg.recoil && firing) {
        const WeaponProf& wp = prof(1.f);
        float scale = g_cfg.recoilComp;
        float v = wp.v * scale;
        targetY += v * dt;
        targetX += sinf((float)(g_game.timer.elapsed) * 7.3f) * wp.h * scale * dt;
        idle_ = 0;
    }

    // ---------- 视角辅助（软吸附，只推一点点） ----------
    if (g_cfg.aimAssist) {
        const Player* best = nullptr;
        float bestD = 1e9f;
        float fov = g_cfg.aimFov;
        float cx = g_scr.w * 0.5f, cy = g_scr.h * 0.5f;
        for (auto& p : g_game.players) {
            if (!p.visible || p.hp <= 0) continue;
            if (g_game.localTeam >= 0 && p.team == g_game.localTeam) continue;
            if (!p.bones[B_CHEST].on) continue;
            float dx = p.bones[B_CHEST].scr.x - cx;
            float dy = p.bones[B_CHEST].scr.y - cy;
            float d = sqrtf(dx * dx + dy * dy);
            if (d < fov * (g_scr.w / 1080.f) * 6.f && d < bestD) {
                bestD = d; best = &p;
            }
        }
        if (best) {
            float dx = best->bones[B_CHEST].scr.x - cx;
            float dy = best->bones[B_CHEST].scr.y - cy;
            // 屏幕坐标 -> 视角拖动量（反向：把目标拉到准星）
            targetX += dx * g_cfg.aimSpeed * dt * 3.f;
            targetY += dy * g_cfg.aimSpeed * dt * 3.f;
        }
    }

    // 平滑跟随
    float damp = g_cfg.recoilDamp < 0.05f ? 0.05f : g_cfg.recoilDamp;
    curX_ += targetX * (1.f - damp * 0.5f);
    curY_ += targetY * (1.f - damp * 0.5f);
    offX_ += curX_;
    offY_ += curY_;
    curX_ *= 0.75f;
    curY_ *= 0.75f;

    // 限制最大位移
    float maxOff = g_scr.h * 0.30f;
    if (offX_ >  maxOff) offX_ =  maxOff;
    if (offX_ < -maxOff) offX_ = -maxOff;
    if (offY_ >  maxOff) offY_ =  maxOff;
    if (offY_ < -maxOff) offY_ = -maxOff;

    // 停火后缓慢复位
    if (!firing && !g_cfg.aimAssist) {
        offX_ *= 0.90f;
        offY_ *= 0.90f;
        if (fabsf(offX_) < 0.5f) offX_ = 0;
        if (fabsf(offY_) < 0.5f) offY_ = 0;
    }

    if (firing || fabsf(offX_) > 0.4f || fabsf(offY_) > 0.4f) {
        if (!active_) {
            g_touch.down(slot_, (int32_t)bx, (int32_t)by);
            active_ = true;
        }
        g_touch.move(slot_, (int32_t)(bx + offX_), (int32_t)(by + offY_));
    } else if (active_) {
        g_touch.up(slot_);
        active_ = false;
    }
}
