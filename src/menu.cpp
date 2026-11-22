// menu.cpp — 菜单界面
#include "imgui.h"
#include "config.hpp"
#include "game.hpp"
#include "touch.hpp"
#include "stealth.hpp"
#include <cstdio>
#include <cstring>

extern Config g_cfg;
void AuxSaveConfig();

static void help(const char* t) {
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", t);
}

static void SectionVis() {
    ImGui::Checkbox(u8"总开关", &g_cfg.master);
    ImGui::SameLine();
    ImGui::Checkbox(u8"骨骼", &g_cfg.skeleton);
    ImGui::SameLine();
    ImGui::Checkbox(u8"方框", &g_cfg.box);

    ImGui::Checkbox(u8"血条", &g_cfg.health);
    ImGui::SameLine();
    ImGui::Checkbox(u8"名字", &g_cfg.nameTag);
    ImGui::SameLine();
    ImGui::Checkbox(u8"距离", &g_cfg.distance);

    ImGui::Checkbox(u8"姿态", &g_cfg.posture);
    ImGui::SameLine();
    ImGui::Checkbox(u8"头部圈", &g_cfg.headDot);
    ImGui::SameLine();
    ImGui::Checkbox(u8"详情面板", &g_cfg.detailPanel);

    ImGui::Checkbox(u8"显示队友", &g_cfg.teamVisible);

    ImGui::Separator();
    ImGui::Text(u8"雷达");
    ImGui::Checkbox(u8"雷达", &g_cfg.radar);
    ImGui::SameLine();
    ImGui::Checkbox(u8"HUD", &g_cfg.hud);
    ImGui::SameLine();
    ImGui::Checkbox(u8"准星", &g_cfg.crosshair);

    ImGui::Checkbox(u8"物资", &g_cfg.loot);

    ImGui::Separator();
    ImGui::SliderFloat(u8"人物距离", &g_cfg.maxDist, 20.f, 800.f, "%.0f m");
    ImGui::SliderFloat(u8"物资距离", &g_cfg.lootDist, 10.f, 400.f, "%.0f m");
    ImGui::SliderFloat(u8"雷达范围", &g_cfg.radarRange, 30.f, 500.f, "%.0f m");
    ImGui::SliderFloat(u8"骨骼粗细", &g_cfg.boneThick, 0.5f, 4.0f, "%.1f");

    ImGui::Separator();
    ImGui::ColorEdit4(u8"真人", g_cfg.colEnemy, ImGuiColorEditFlags_NoInputs);
    ImGui::SameLine();
    ImGui::ColorEdit4(u8"AI", g_cfg.colBot, ImGuiColorEditFlags_NoInputs);
    ImGui::SameLine();
    ImGui::ColorEdit4(u8"队友", g_cfg.colTeam, ImGuiColorEditFlags_NoInputs);
}

static void SectionCombat() {
    ImGui::Text(u8"触摸压枪（uinput，独立 pointer，不写游戏内存）");
    ImGui::Checkbox(u8"启用压枪", &g_cfg.recoil);
    ImGui::SliderFloat(u8"补偿强度", &g_cfg.recoilComp, 0.1f, 3.0f, "%.2f");
    ImGui::SliderFloat(u8"平滑", &g_cfg.recoilDamp, 0.1f, 0.98f, "%.2f");

    ImGui::Separator();
    ImGui::Text(u8"视角辅助");
    ImGui::Checkbox(u8"启用", &g_cfg.aimAssist);
    ImGui::SliderFloat(u8"FOV", &g_cfg.aimFov, 1.f, 60.f, "%.0f");
    ImGui::SliderFloat(u8"速度", &g_cfg.aimSpeed, 0.05f, 1.0f, "%.2f");

    ImGui::Separator();
    ImGui::Text(u8"触摸注入状态: %s", g_touch.ok() ? "就绪" : "未就绪(/dev/uinput)");
}

static void SectionDebug() {
    auto& s = g_game.schema;
    ImGui::Text(u8"il2cpp: %s", g_il2.ok ? "OK" : "FAIL");
    ImGui::Text(u8"schema : %s", s.ok ? "OK" : "FAIL");
    ImGui::Text(u8"玩家类 : %s", s.playerCls ? s.playerCls : "-");
    ImGui::Text(u8"world  : %p", s.worldGetter);
    ImGui::Text(u8"PB.info 0x%llx  PB.tr 0x%llx  PB.skel 0x%llx",
                (unsigned long long)s.PB_info,
                (unsigned long long)s.PB_transform,
                (unsigned long long)s.PB_skeleton);
    ImGui::Text(u8"PI.pos 0x%llx  PI.hp 0x%llx  PI.team 0x%llx",
                (unsigned long long)s.PI_pos,
                (unsigned long long)s.PI_hp,
                (unsigned long long)s.PI_team);
    ImGui::Separator();
    ImGui::Text(u8"本地坐标 %.1f %.1f %.1f", g_game.localPos.x,
                g_game.localPos.y, g_game.localPos.z);
    ImGui::Text(u8"玩家数 %zu   物资数 %zu", g_game.players.size(), g_game.loots.size());
    ImGui::Text(u8"开火 %d   弹药 %d", (int)g_game.firing, g_game.ammo);
    ImGui::Text(u8"相机 %p", (void*)g_cam);

    ImGui::Separator();
    if (ImGui::Button(u8"重新探测 schema")) {
        g_game.init();
    }
    ImGui::SameLine();
    if (ImGui::Button(u8"保存配置")) AuxSaveConfig();
    ImGui::SameLine();
    if (ImGui::Button(u8"清理分析器")) stealth::killAnalyzers();
}

void DrawMenu() {
    if (!g_cfg.menu) return;

    ImGui::SetNextWindowSize({ g_cfg.menuW, g_cfg.menuH }, ImGuiCond_Once);
    ImGui::SetNextWindowPos({ g_cfg.menuX, g_cfg.menuY }, ImGuiCond_Once);
    ImGui::SetNextWindowBgAlpha(g_cfg.menuAlpha);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.04f, 0.07f, 0.06f, g_cfg.menuAlpha));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.08f, 0.20f, 0.14f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.16f, 1.f, 0.35f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.16f, 0.95f, 0.35f, 1.f));

    if (ImGui::Begin(u8"AUX 视觉套件  v" AUX_VERSION, &g_cfg.menu,
                     ImGuiWindowFlags_NoCollapse)) {

        if (ImGui::BeginTabBar("tab")) {
            if (ImGui::BeginTabItem(u8"视觉")) {
                SectionVis();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(u8"战斗")) {
                SectionCombat();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(u8"诊断")) {
                SectionDebug();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        ImGui::Separator();
        ImGui::TextDisabled(u8"音量上+下 同时按 = 开关菜单   拖动标题栏移动");
        ImGui::TextDisabled(u8"FPS %.1f", ImGui::GetIO().Framerate);
    }
    ImGui::End();

    ImGui::PopStyleColor(4);
}
