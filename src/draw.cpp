// draw.cpp — 视觉渲染层
#include "imgui.h"
#include "game.hpp"
#include "config.hpp"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <vector>

extern Config g_cfg;

static ImU32 C4(const float* c) {
    return IM_COL32((int)(c[0] * 255), (int)(c[1] * 255),
                    (int)(c[2] * 255), (int)(c[3] * 255));
}

static ImU32 teamColor(const Player& p) {
    if (g_cfg.teamVisible && g_game.localTeam >= 0 && p.team == g_game.localTeam)
        return C4(g_cfg.colTeam);
    return p.isPerson ? C4(g_cfg.colEnemy) : C4(g_cfg.colBot);
}

// ---------------- 骨骼 ----------------
static void line(ImDrawList* dl, const Player& p, int a, int b,
                 ImU32 c, float t) {
    if (!p.bones[a].on || !p.bones[b].on) return;
    dl->AddLine({ p.bones[a].scr.x, p.bones[a].scr.y },
                { p.bones[b].scr.x, p.bones[b].scr.y }, c, t);
}

static void DrawSkeleton(ImDrawList* dl, const Player& p, ImU32 c) {
    float t = g_cfg.boneThick;
    line(dl, p, B_HEAD,   B_NECK,   c, t);
    line(dl, p, B_NECK,   B_CHEST,  c, t);
    line(dl, p, B_CHEST,  B_SPINE,  c, t);
    line(dl, p, B_SPINE,  B_PELVIS, c, t);
    line(dl, p, B_NECK,   B_LSHLDR, c, t);
    line(dl, p, B_LSHLDR, B_LELBOW, c, t);
    line(dl, p, B_LELBOW, B_LHAND,  c, t);
    line(dl, p, B_NECK,   B_RSHLDR, c, t);
    line(dl, p, B_RSHLDR, B_RELBOW, c, t);
    line(dl, p, B_RELBOW, B_RHAND,  c, t);
    line(dl, p, B_PELVIS, B_LHIP,   c, t);
    line(dl, p, B_LHIP,   B_LKNEE,  c, t);
    line(dl, p, B_LKNEE,  B_LFOOT,  c, t);
    line(dl, p, B_PELVIS, B_RHIP,   c, t);
    line(dl, p, B_RHIP,   B_RKNEE,  c, t);
    line(dl, p, B_RKNEE,  B_RFOOT,  c, t);

    if (g_cfg.headDot && p.bones[B_HEAD].on) {
        float r = std::max(3.5f, (p.boxB - p.boxT) * 0.085f);
        dl->AddCircle({ p.bones[B_HEAD].scr.x, p.bones[B_HEAD].scr.y },
                      r, c, 16, t);
    }
}

// ---------------- 方框 + 血条 + 文字 ----------------
static void DrawBox(ImDrawList* dl, const Player& p, ImU32 c) {
    if (!p.visible) return;

    // 四角方框（比整框更像游戏自带）
    float L = p.boxL, T = p.boxT, R = p.boxR, B = p.boxB;
    float cw = (R - L) * 0.22f, ch = (B - T) * 0.12f;
    float t = g_cfg.boxThick;
    dl->AddLine({ L, T }, { L + cw, T }, c, t);
    dl->AddLine({ L, T }, { L, T + ch }, c, t);
    dl->AddLine({ R, T }, { R - cw, T }, c, t);
    dl->AddLine({ R, T }, { R, T + ch }, c, t);
    dl->AddLine({ L, B }, { L + cw, B }, c, t);
    dl->AddLine({ L, B }, { L, B - ch }, c, t);
    dl->AddLine({ R, B }, { R - cw, B }, c, t);
    dl->AddLine({ R, B }, { R, B - ch }, c, t);

    // 血条
    if (g_cfg.health) {
        float bw = std::max(2.5f, (R - L) * 0.10f);
        float hp = p.maxHp > 1.f ? p.hp / p.maxHp : 0.f;
        hp = std::clamp(hp, 0.f, 1.f);
        float bh = (B - T) * hp;
        ImU32 hc = hp > 0.6f ? IM_COL32(60, 220, 90, 235)
                 : hp > 0.3f ? IM_COL32(240, 200, 50, 235)
                             : IM_COL32(235, 60, 60, 235);
        dl->AddRectFilled({ L - bw - 3, T }, { L - 3, B }, IM_COL32(0, 0, 0, 150));
        dl->AddRectFilled({ L - bw - 3, B - bh }, { L - 3, B }, hc);
    }

    // 名字 / 距离
    char buf[128];
    if (g_cfg.nameTag) {
        const char* nm = p.name.empty() ? (p.isPerson ? "PMC" : "BOT") : p.name.c_str();
        snprintf(buf, sizeof buf, "%s", nm);
        ImVec2 ts = ImGui::CalcTextSize(buf);
        dl->AddText({ (L + R) * 0.5f - ts.x * 0.5f, T - ts.y - 4 },
                    IM_COL32(255, 255, 255, 235), buf);
    }
    if (g_cfg.distance) {
        snprintf(buf, sizeof buf, "%d m", (int)p.dist);
        ImVec2 ts = ImGui::CalcTextSize(buf);
        dl->AddText({ (L + R) * 0.5f - ts.x * 0.5f, B + 3 },
                    IM_COL32(200, 235, 255, 220), buf);
    }
    if (g_cfg.posture) {
        const char* st = p.state == 2 ? "PRONE" : p.state == 1 ? "CROUCH" : "STAND";
        dl->AddText({ L, B + 18 }, IM_COL32(170, 210, 255, 200), st);
    }
}

// ---------------- 雷达 ----------------
static void DrawRadar(ImDrawList* dl, ImVec2 c, float r) {
    float range = std::max(10.f, g_cfg.radarRange);

    dl->AddCircleFilled(c, r, IM_COL32(8, 14, 18, 170), 64);
    dl->AddCircle(c, r, IM_COL32(120, 210, 160, 200), 64, 1.6f);
    dl->AddCircle(c, r * 0.66f, IM_COL32(70, 130, 110, 110), 48, 1.f);
    dl->AddCircle(c, r * 0.33f, IM_COL32(70, 130, 110, 110), 48, 1.f);
    dl->AddLine({ c.x - r, c.y }, { c.x + r, c.y }, IM_COL32(50, 110, 90, 90));
    dl->AddLine({ c.x, c.y - r }, { c.x, c.y + r }, IM_COL32(50, 110, 90, 90));

    // 玩家自身箭头
    dl->AddTriangleFilled({ c.x, c.y - 7 }, { c.x - 5, c.y + 5 }, { c.x + 5, c.y + 5 },
                          IM_COL32(255, 255, 255, 245));

    for (const auto& p : g_game.players) {
        if (p.dist > range) continue;
        Vec2 rp = RadarMap(g_game.localPos, g_game.localYaw, p.origin, range, r);
        if (fabsf(rp.x) > r || fabsf(rp.y) > r) continue;
        ImU32 col = p.isPerson ? IM_COL32(60, 255, 110, 245)
                               : IM_COL32(255, 200, 70, 225);
        dl->AddCircleFilled({ c.x + rp.x, c.y + rp.y }, 3.4f, col, 12);
        if (p.isPerson)
            dl->AddCircle({ c.x + rp.x, c.y + rp.y }, 6.5f, col, 12, 1.f);
    }

    // 方位刻度
    const char* dirs[] = { "N", "E", "S", "W" };
    for (int i = 0; i < 4; ++i) {
        float a = i * 90.f * 3.14159265f / 180.f;
        float x = c.x + sinf(a) * (r + 9);
        float y = c.y - cosf(a) * (r + 9);
        ImVec2 ts = ImGui::CalcTextSize(dirs[i]);
        dl->AddText({ x - ts.x * 0.5f, y - ts.y * 0.5f },
                    IM_COL32(150, 230, 190, 200), dirs[i]);
    }
}

// ---------------- HUD ----------------
static void DrawHUD(ImDrawList* dl, float sw, float sh) {
    char b[64];

    // 剩余时间
    snprintf(b, sizeof b, "剩余时间  %02d:%02d",
             g_game.timer.remain / 60, g_game.timer.remain % 60);
    dl->AddText({ 20, 16 }, IM_COL32(255, 236, 140, 240), b);

    // 顶部罗盘
    float cx = sw * 0.5f;
    const char* dirs[] = { "北", "", "东北", "", "东", "", "东南", "",
                           "南", "", "西南", "", "西", "", "西北", "" };
    for (int i = -7; i <= 7; ++i) {
        int deg = ((int)g_game.localYaw + 360 + i * 15) % 360;
        float x = cx + i * 36.f;
        char cb[8];
        if (deg % 45 == 0) snprintf(cb, sizeof cb, "%s", dirs[deg / 45]);
        else               snprintf(cb, sizeof cb, "%d", deg);
        ImVec2 ts = ImGui::CalcTextSize(cb);
        dl->AddText({ x - ts.x * 0.5f, 10 }, IM_COL32(232, 232, 232, 225), cb);
    }
    dl->AddTriangleFilled({ cx, 42 }, { cx - 5, 32 }, { cx + 5, 32 },
                          IM_COL32(255, 220, 120, 255));

    // 左上姿态剪影
    ImVec2 sp{ 58, 66 };
    ImU32 sk = IM_COL32(238, 242, 238, 225);
    float k = g_game.localState == 2 ? 0.55f : g_game.localState == 1 ? 0.78f : 1.f;
    dl->AddCircle({ sp.x, sp.y - 24 * k }, 7.5f * k, sk, 18, 2.f);
    dl->AddLine({ sp.x, sp.y - 16 * k }, { sp.x, sp.y + 12 * k }, sk, 3.f);
    dl->AddLine({ sp.x, sp.y - 11 * k }, { sp.x - 13 * k, sp.y + 1 * k }, sk, 2.5f);
    dl->AddLine({ sp.x, sp.y - 11 * k }, { sp.x + 13 * k, sp.y + 1 * k }, sk, 2.5f);
    dl->AddLine({ sp.x, sp.y + 12 * k }, { sp.x - 10 * k, sp.y + 28 * k }, sk, 3.f);
    dl->AddLine({ sp.x, sp.y + 12 * k }, { sp.x + 10 * k, sp.y + 28 * k }, sk, 3.f);

    // 右上网速
    snprintf(b, sizeof b, "%d ms", g_game.ping);
    ImVec2 ts2 = ImGui::CalcTextSize(b);
    dl->AddText({ sw - ts2.x - 18, sh - 30 }, IM_COL32(160, 235, 165, 210), b);

    // 敌人计数
    int human = 0, bot = 0;
    for (auto& p : g_game.players) {
        if (!p.visible || p.dist > g_cfg.maxDist) continue;
        p.isPerson ? ++human : ++bot;
    }
    snprintf(b, sizeof b, "PMC %d   BOT %d", human, bot);
    dl->AddText({ 20, 42 }, IM_COL32(190, 230, 255, 220), b);
}

// ---------------- 详情面板（截图显示器那块） ----------------
static void DrawDetail(ImDrawList* dl, const Player& p) {
    ImVec2 pos{ g_scr.w * 0.14f, g_scr.h * 0.26f };
    ImVec2 sz{ 190, 132 };

    dl->AddRectFilled(pos, { pos.x + sz.x, pos.y + sz.y },
                      IM_COL32(6, 12, 10, 195), 5.f);
    dl->AddRect(pos, { pos.x + sz.x, pos.y + sz.y },
                IM_COL32(80, 210, 135, 190), 5.f, 0, 1.2f);

    // 迷你骨骼
    float cx = pos.x + sz.x * 0.30f, cy = pos.y + sz.y * 0.52f;
    float s = 42.f;
    Vec3 o = p.origin;
    auto P = [&](int i) {
        return ImVec2{ cx + (p.bones[i].pos.x - o.x) * s,
                       cy - (p.bones[i].pos.y - o.y) * s };
    };
    ImU32 lc = IM_COL32(120, 255, 170, 225);
    auto L2 = [&](int a, int b) { dl->AddLine(P(a), P(b), lc, 1.3f); };
    L2(B_HEAD, B_NECK); L2(B_NECK, B_CHEST); L2(B_CHEST, B_PELVIS);
    L2(B_NECK, B_LHAND); L2(B_NECK, B_RHAND);
    L2(B_PELVIS, B_LFOOT); L2(B_PELVIS, B_RFOOT);

    char b[128];
    snprintf(b, sizeof b, "%s", p.name.empty() ? (p.isPerson ? "PMC" : "BOT") : p.name.c_str());
    dl->AddText({ pos.x + sz.x * 0.60f, pos.y + 14 }, IM_COL32(205, 255, 225, 235), b);
    snprintf(b, sizeof b, "HP   %d / %d", (int)p.hp, (int)p.maxHp);
    dl->AddText({ pos.x + sz.x * 0.60f, pos.y + 36 }, IM_COL32(255, 210, 120, 225), b);
    snprintf(b, sizeof b, "DIST %d m", (int)p.dist);
    dl->AddText({ pos.x + sz.x * 0.60f, pos.y + 56 }, IM_COL32(160, 225, 255, 225), b);
    snprintf(b, sizeof b, "TEAM %d", p.team);
    dl->AddText({ pos.x + sz.x * 0.60f, pos.y + 76 }, IM_COL32(215, 165, 255, 225), b);
    snprintf(b, sizeof b, "%s", p.isPerson ? "HUMAN" : "AI");
    dl->AddText({ pos.x + sz.x * 0.60f, pos.y + 96 }, IM_COL32(160, 255, 190, 225), b);
}

// ---------------- 物资 ----------------
static void DrawLoot(ImDrawList* dl, const Vec3& cameraPos) {
    for (const auto& l : g_game.loots) {
        if (l.dist > g_cfg.lootDist || l.dist < 0.5f) continue;
        Vec3 s{};
        if (!g_game.w2s(l.pos, s)) continue;

        ImU32 c = l.rarity >= 4 ? IM_COL32(255, 120, 255, 235)
                : l.rarity == 3 ? IM_COL32(255, 190, 60, 235)
                : l.rarity == 2 ? IM_COL32(120, 200, 255, 220)
                                : IM_COL32(205, 205, 205, 205);
        ImVec2 p{ s.x, s.y };
        dl->AddRectFilled({ p.x - 8, p.y - 8 }, { p.x + 8, p.y + 8 },
                          IM_COL32(15, 18, 20, 175), 3.f);
        dl->AddRect({ p.x - 8, p.y - 8 }, { p.x + 8, p.y + 8 }, c, 3.f, 0, 1.4f);
        char b[24]; snprintf(b, sizeof b, "%dm", (int)l.dist);
        ImVec2 ts = ImGui::CalcTextSize(b);
        dl->AddText({ p.x - ts.x * 0.5f, p.y + 10 }, c, b);
    }
    (void)cameraPos;
}

// ---------------- 准星 ----------------
static void DrawCrosshair(ImDrawList* dl, float sw, float sh) {
    ImVec2 c{ sw * 0.5f, sh * 0.5f };
    ImU32 col = IM_COL32(120, 255, 170, 200);
    dl->AddLine({ c.x - 11, c.y }, { c.x - 4, c.y }, col, 1.5f);
    dl->AddLine({ c.x + 4, c.y }, { c.x + 11, c.y }, col, 1.5f);
    dl->AddLine({ c.x, c.y - 11 }, { c.x, c.y - 4 }, col, 1.5f);
    dl->AddLine({ c.x, c.y + 4 }, { c.x, c.y + 11 }, col, 1.5f);
    dl->AddCircle(c, 2.f, col, 8, 1.2f);
}

// ---------------- 总入口 ----------------
void RenderESP() {
    if (!g_cfg.master) return;
    if (g_game.busy.load(std::memory_order_relaxed)) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    float sw = (float)g_scr.w, sh = (float)g_scr.h;

    // 按距离排序，远的先画
    std::vector<const Player*> list;
    list.reserve(g_game.players.size());
    for (auto& p : g_game.players) {
        if (!p.visible) continue;
        if (p.dist > g_cfg.maxDist) continue;
        if (!g_cfg.teamVisible && g_game.localTeam >= 0 && p.team == g_game.localTeam) continue;
        list.push_back(&p);
    }
    std::sort(list.begin(), list.end(),
              [](const Player* a, const Player* b) { return a->dist > b->dist; });

    const Player* nearest = nullptr;
    for (const Player* p : list) {
        ImU32 c = teamColor(*p);
        if (g_cfg.skeleton) DrawSkeleton(dl, *p, c);
        if (g_cfg.box)      DrawBox(dl, *p, c);
        if (!nearest || p->dist < nearest->dist) nearest = p;
    }

    if (g_cfg.detailPanel && nearest && nearest->dist < 60.f)
        DrawDetail(dl, *nearest);

    if (g_cfg.loot)  DrawLoot(dl, g_game.localPos);
    if (g_cfg.radar) DrawRadar(dl, { sw - 105.f, 105.f }, 80.f);
    if (g_cfg.hud)   DrawHUD(dl, sw, sh);
    if (g_cfg.crosshair) DrawCrosshair(dl, sw, sh);
}
