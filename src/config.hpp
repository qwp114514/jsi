#pragma once
#include <cstdint>
#include <string>

struct Config {
    // ===== 视觉 =====
    bool  master     = true;
    bool  skeleton   = true;
    bool  box        = true;
    bool  health     = true;
    bool  nameTag    = true;
    bool  distance   = true;
    bool  posture    = true;
    bool  headDot    = true;
    bool  detailPanel= true;
    bool  loot       = true;
    bool  lootFilter = false;
    bool  radar      = true;
    bool  hud        = true;
    bool  weaponIcon = true;
    bool  crosshair  = true;
    bool  teamVisible= false;   // 是否显示队友

    float maxDist    = 300.f;
    float lootDist   = 120.f;
    float radarRange = 150.f;
    float boneThick  = 1.6f;
    float boxThick   = 1.4f;
    float fontSize   = 15.f;

    float colEnemy[4] = { 0.16f, 1.00f, 0.28f, 0.92f };
    float colBot[4]   = { 1.00f, 0.75f, 0.24f, 0.86f };
    float colTeam[4]  = { 0.35f, 0.78f, 1.00f, 0.90f };

    // ===== 战斗 =====
    bool  recoil     = false;      // 触摸压枪
    float recoilComp = 1.0f;       // 补偿系数
    float recoilDamp = 0.85f;      // 平滑
    bool  aimAssist  = false;      // 视角辅助（触摸注入）
    float aimFov     = 12.f;
    float aimSpeed   = 0.35f;

    // ===== 界面 =====
    bool  menu       = false;
    int   tab        = 0;
    float menuW      = 520.f;
    float menuH      = 420.f;
    float menuX      = 60.f;
    float menuY      = 60.f;
    float menuAlpha  = 0.95f;

    void load(const char* path);
    void save(const char* path);
};

extern Config g_cfg;
