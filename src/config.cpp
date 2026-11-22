#include "config.hpp"
#include "resolver.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>

Config g_cfg;

#define KV_B(name, field) if (strcmp(k, #name) == 0) { field = atoi(v); continue; }
#define KV_I(name, field) if (strcmp(k, #name) == 0) { field = atoi(v); continue; }
#define KV_F(name, field) if (strcmp(k, #name) == 0) { field = (float)atof(v); continue; }
#define KV_C(name, field) if (strcmp(k, #name) == 0) { \
    sscanf(v, "%f,%f,%f,%f", &field[0], &field[1], &field[2], &field[3]); continue; }

void Config::load(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof line, f)) {
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        char* k = line;
        char* v = eq + 1;
        while (*k == ' ' || *k == '\t') ++k;
        size_t kl = strlen(k);
        while (kl && (k[kl-1] == ' ' || k[kl-1] == '\n' || k[kl-1] == '\r')) k[--kl] = 0;
        while (*v == ' ' || *v == '\t') ++v;
        size_t vl = strlen(v);
        while (vl && (v[vl-1] == ' ' || v[vl-1] == '\n' || v[vl-1] == '\r')) v[--vl] = 0;

        KV_B(master, master)
        KV_B(skeleton, skeleton)
        KV_B(box, box)
        KV_B(health, health)
        KV_B(nameTag, nameTag)
        KV_B(distance, distance)
        KV_B(posture, posture)
        KV_B(headDot, headDot)
        KV_B(detailPanel, detailPanel)
        KV_B(loot, loot)
        KV_B(radar, radar)
        KV_B(hud, hud)
        KV_B(weaponIcon, weaponIcon)
        KV_B(crosshair, crosshair)
        KV_B(teamVisible, teamVisible)
        KV_B(recoil, recoil)
        KV_B(aimAssist, aimAssist)
        KV_F(maxDist, maxDist)
        KV_F(lootDist, lootDist)
        KV_F(radarRange, radarRange)
        KV_F(boneThick, boneThick)
        KV_F(boxThick, boxThick)
        KV_F(fontSize, fontSize)
        KV_F(recoilComp, recoilComp)
        KV_F(recoilDamp, recoilDamp)
        KV_F(aimFov, aimFov)
        KV_F(aimSpeed, aimSpeed)
        KV_F(menuAlpha, menuAlpha)
        KV_I(tab, tab)
        KV_C(colEnemy, colEnemy)
        KV_C(colBot, colBot)
        KV_C(colTeam, colTeam)
    }
    fclose(f);
    LOGI("config loaded from %s", path);
}

void Config::save(const char* path) {
    FILE* f = fopen(path, "w");
    if (!f) return;
#define W_B(n) fprintf(f, #n "=%d\n", (int)n)
#define W_I(n) fprintf(f, #n "=%d\n", (int)n)
#define W_F(n) fprintf(f, #n "=%.4f\n", (double)n)
#define W_C(n) fprintf(f, #n "=%.3f,%.3f,%.3f,%.3f\n", n[0], n[1], n[2], n[3])
    W_B(master); W_B(skeleton); W_B(box); W_B(health); W_B(nameTag);
    W_B(distance); W_B(posture); W_B(headDot); W_B(detailPanel); W_B(loot);
    W_B(radar); W_B(hud); W_B(weaponIcon); W_B(crosshair); W_B(teamVisible);
    W_B(recoil); W_B(aimAssist);
    W_F(maxDist); W_F(lootDist); W_F(radarRange); W_F(boneThick); W_F(boxThick);
    W_F(fontSize); W_F(recoilComp); W_F(recoilDamp); W_F(aimFov); W_F(aimSpeed);
    W_F(menuAlpha);
    W_I(tab);
    W_C(colEnemy); W_C(colBot); W_C(colTeam);
    fclose(f);
}
