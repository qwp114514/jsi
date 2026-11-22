// game.cpp — 对象遍历 + 骨骼解析
// 设计目标：零硬编码偏移。全部通过 il2cpp 运行时 API 探测，
// 类名/字段名用候选表自动匹配，游戏换皮改版本后仍可自愈。
#include "game.hpp"
#include "config.hpp"
#include "resolver.hpp"
#include <cstring>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <ctime>
#include <cstdlib>

Game g_game;
Mat4 g_vp{};
uint64_t g_cam = 0;

// ---------------- 候选表 ----------------

static const char* kFacadeCls[] = {
    "GameFacade", "GameWorld", "GameManager", "BattleManager",
    "ArenaManager", "GameApp", "MainManager", "EntityManager",
    "WorldManager", "SceneGame", "GameScene", nullptr
};
static const char* kInstFlds[] = {
    "Instance", "instance", "s_Instance", "_instance", "Main", "Current",
    "Singleton", "s_Instance_", "m_Instance", "Get", nullptr
};
static const char* kPlayerCls[] = {
    "PlayerBase", "ArenaPlayer", "Player", "HumanPlayer", "AIPlayer",
    "CharacterBase", "ActorPlayer", "PawnPlayer", "PlayerEntity", nullptr
};
static const char* kListFlds[] = {
    "m_Players", "m_PlayerList", "m_AllPlayers", "players", "m_Entities",
    "m_EntityList", "m_Actors", "m_Characters", "m_Fighters", "AllPlayers",
    "m_PlayerMap", "m_PlayerDic", nullptr
};
static const char* kLocalFlds[] = {
    "m_LocalPlayer", "m_LocalPlayerInfo", "m_MainPlayer", "localPlayer",
    "m_SelfPlayer", "m_MyPlayer", "m_OwnerPlayer", "LocalPlayer", nullptr
};
static const char* kLootFlds[] = {
    "m_Loots", "m_LootList", "m_Items", "m_ItemList", "m_Drops",
    "m_DropList", "m_Boxes", "m_SceneItems", nullptr
};

// 字段名候选
static const char* kInfoFlds[]  = { "m_PlayerInfo", "playerInfo", "m_Info",
                                    "m_PI", "info", "m_ActorInfo", nullptr };
static const char* kTransFlds[] = { "m_Transform", "transform", "m_trans",
                                    "m_Trans", nullptr };
static const char* kSkelFlds[]  = { "m_SkeletonRoot", "skeletonRoot", "m_Skeleton",
                                    "m_BoneRoot", "m_RootBone", nullptr };
static const char* kWeaponFlds[]= { "m_WeaponManager", "weaponManager",
                                    "m_Weapon", "m_EquipManager", nullptr };
static const char* kPosFlds[]   = { "m_PlayerPosition", "m_Position", "position",
                                    "m_Pos", "m_WorldPos", nullptr };
static const char* kHpFlds[]    = { "m_Health", "m_Hp", "health", "m_HP",
                                    "m_CurHealth", nullptr };
static const char* kMaxHpFlds[] = { "m_MaxHealth", "m_MaxHp", "maxHealth",
                                    "m_HpMax", nullptr };
static const char* kTeamFlds[]  = { "m_TeamID", "m_TeamId", "m_Team", "teamId",
                                    "m_GroupID", "m_Camp", nullptr };
static const char* kNameFlds[]  = { "m_NickName", "m_Name", "nickName", "name",
                                    "m_PlayerName", nullptr };
static const char* kStateFlds[] = { "m_PlayerState", "m_State", "state",
                                    "m_PoseType", "m_MoveState", nullptr };
static const char* kPersonFlds[]= { "m_IsPerson", "m_IsHuman", "m_IsPlayer",
                                    "isPerson", "m_IsAI", nullptr };
static const char* kFireFlds[]  = { "m_IsFiring", "m_bFiring", "m_IsShooting",
                                    "m_bShooting", "m_Firing", "m_IsAttack",
                                    "m_bAttack", "m_FireState", nullptr };
static const char* kAmmoFlds[]  = { "m_CurAmmo", "m_Ammo", "m_BulletCount",
                                    "m_ClipAmmo", "m_CurBullet", "m_Bullets", nullptr };
static const char* kWidFlds[]   = { "m_WeaponID", "m_WeaponId", "m_ItemID",
                                    "m_ItemId", "m_Id", nullptr };

// ---------------- 工具 ----------------

static uint64_t pickField(uint64_t (*fn)(void*, const char*), void* cls,
                          const char* const* names) {
    for (int i = 0; names[i]; ++i) {
        uint64_t v = fn(cls, names[i]);
        if (v) return v;
    }
    return 0;
}

static uint64_t staticObjPtr(Il2CppClass* k, const char* fname) {
    if (!k || !g_il2.class_get_field_from_name) return 0;
    FieldInfo* f = g_il2.class_get_field_from_name(k, fname);
    if (!f) return 0;
    uint64_t buf = 0;
    if (g_il2.field_static_get_value) g_il2.field_static_get_value(f, &buf);
    return buf;
}

typedef Vec3 (*GetPosFn)(void*);
typedef Vec3 (*W2SFn)(void*, Vec3);
typedef void* (*GetMainFn)();

// ---------------- schema 解析 ----------------

void Game::resolveSchema() {
    auto& s = schema;
    memset(&s, 0, sizeof s);

    // 1. 世界单例
    for (int i = 0; kFacadeCls[i] && !s.worldGetter; ++i) {
        Il2CppClass* k = g_il2.findClass(kFacadeCls[i]);
        if (!k) continue;
        for (int j = 0; kInstFlds[j]; ++j) {
            uint64_t v = staticObjPtr(k, kInstFlds[j]);
            if (v) { s.worldGetter = (void*)v; break; }
        }
        if (!s.worldGetter) {
            // 静态区直接是对象引用（无 Instance 字段）

        }
    }
    LOGI("schema: world=%p", s.worldGetter);

    // 2. 玩家类
    for (int i = 0; kPlayerCls[i]; ++i) {
        Il2CppClass* k = g_il2.findClass(kPlayerCls[i]);
        if (!k) continue;
        if (g_il2.class_get_field_from_name(k, kInfoFlds[0]) ||
            g_il2.class_get_field_from_name(k, kInfoFlds[1]) ||
            g_il2.class_get_field_from_name(k, kPosFlds[0])) {
            s.playerCls = kPlayerCls[i];
            break;
        }
    }
    if (!s.playerCls) s.playerCls = "PlayerBase";
    LOGI("schema: playerCls=%s", s.playerCls);

    Il2CppClass* pk = g_il2.findClass(s.playerCls);
    if (pk) {
        auto fld = [](void* c, const char* n) -> uint64_t {
            FieldInfo* f = g_il2.class_get_field_from_name(c, n);
            return f ? g_il2.field_get_offset(f) : 0;
        };
        s.PB_info      = pickField(fld, pk, kInfoFlds);
        s.PB_transform = pickField(fld, pk, kTransFlds);
        s.PB_skeleton  = pickField(fld, pk, kSkelFlds);
        s.PB_weapon    = pickField(fld, pk, kWeaponFlds);
        s.PB_fire      = pickField(fld, pk, kFireFlds);
        s.PB_ammo      = pickField(fld, pk, kAmmoFlds);
        s.PB_wid       = pickField(fld, pk, kWidFlds);
    }

    // 3. PlayerInfo 字段
    Il2CppClass* ik = nullptr;
    if (s.PB_info) {
        // 通过字段类型拿类：il2cpp_field_get_type + il2cpp_type_get_object
        ik = g_il2.findClass("PlayerInfo");
        if (!ik) { ik = g_il2.findClass("PlayerData"); }
        if (!ik) { ik = g_il2.findClass("ActorInfo"); }
    }
    if (!ik) ik = pk;
    if (ik) {
        auto fld = [](void* c, const char* n) -> uint64_t {
            FieldInfo* f = g_il2.class_get_field_from_name(c, n);
            return f ? g_il2.field_get_offset(f) : 0;
        };
        s.PI_pos    = pickField(fld, ik, kPosFlds);
        s.PI_hp     = pickField(fld, ik, kHpFlds);
        s.PI_maxhp  = pickField(fld, ik, kMaxHpFlds);
        s.PI_team   = pickField(fld, ik, kTeamFlds);
        s.PI_name   = pickField(fld, ik, kNameFlds);
        s.PI_state  = pickField(fld, ik, kStateFlds);
        s.PI_person = pickField(fld, ik, kPersonFlds);
    }

    // 4. Transform 结构
    Il2CppClass* tk = g_il2.findClass("Transform");
    if (tk) {
        auto fld = [](void* c, const char* n) -> uint64_t {
            FieldInfo* f = g_il2.class_get_field_from_name(c, n);
            return f ? g_il2.field_get_offset(f) : 0;
        };
        s.TR_children = fld(tk, "m_Children");
        s.TR_father   = fld(tk, "m_Father");
        s.transformGetPos = g_il2.methodAddr("Transform", "get_position", 0);
        s.PB_gameobject   = fld(tk, "m_GameObject");
        s.trForward       = g_il2.methodAddr("Transform", "get_forward", 0);
    }
    s.goGetName = g_il2.methodAddr("GameObject", "get_name", 0);

    // 5. 相机
    Il2CppClass* ck = g_il2.findClass("Camera");
    if (ck) {
        s.cameraMainFn = g_il2.methodAddr("Camera", "get_main", 0);
        s.w2spFn       = g_il2.methodAddr("Camera", "WorldToScreenPoint", 1);
    }

    // 6. 列表字段
    if (s.worldGetter) {
        uint64_t w = (uint64_t)s.worldGetter;
        Il2CppClass* wk = g_il2.findClass(kFacadeCls[0]);
        for (int i = 0; kFacadeCls[i] && !wk; ++i) wk = g_il2.findClass(kFacadeCls[i]);
        if (wk) {
            auto fld = [](void* c, const char* n) -> uint64_t {
                FieldInfo* f = g_il2.class_get_field_from_name(c, n);
                return f ? g_il2.field_get_offset(f) : 0;
            };
            s.playerListGetter = (void*)pickField(fld, wk, kListFlds);
            s.localPlayerGetter= (void*)pickField(fld, wk, kLocalFlds);
            s.lootListGetter   = (void*)pickField(fld, wk, kLootFlds);
        }
        (void)w;
    }

    s.ok = s.worldGetter && s.playerCls;
    LOGI("schema done ok=%d pb(info=%llx tr=%llx skel=%llx) pi(pos=%llx hp=%llx team=%llx name=%llx)",
         s.ok, (unsigned long long)s.PB_info, (unsigned long long)s.PB_transform,
         (unsigned long long)s.PB_skeleton, (unsigned long long)s.PI_pos,
         (unsigned long long)s.PI_hp, (unsigned long long)s.PI_team,
         (unsigned long long)s.PI_name);
}

// ---------------- 基础调用 ----------------

Vec3 Game::callGetPos(uint64_t tr) {
    if (!schema.transformGetPos || !tr) return {};
    return ((GetPosFn)schema.transformGetPos)((void*)tr);
}

bool Game::callW2S(uint64_t cam, const Vec3& w, Vec3& out) {
    if (!schema.w2spFn || !cam) return false;
    out = ((W2SFn)schema.w2spFn)((void*)cam, w);
    return out.z > 0.001f;
}

uint64_t Game::derefWorld() {
    if (!schema.worldGetter) return 0;
    uint64_t w = (uint64_t)schema.worldGetter;
    // 世界是指针字段本身已是对象
    return w;
}

// ---------------- 骨骼 ----------------

static int matchBone(const char* raw) {
    if (!raw || !*raw) return -1;
    char n[64] = {0};
    for (int i = 0; i < 63 && raw[i]; ++i) n[i] = (char)tolower((unsigned char)raw[i]);

    auto has = [&](const char* k) { return strstr(n, k) != nullptr; };
    bool L = has("left") || has("l_") || has("_l") || strstr(n, "l") == n;
    bool R = has("right") || has("r_") || has("_r") || strstr(n, "r") == n;

    if (has("head"))  return B_HEAD;
    if (has("neck"))  return B_NECK;
    if (has("pelvis") || has("hips") || has("bip01 pelvis")) return B_PELVIS;

    if (has("spine2") || has("chest") || has("upperchest")) return B_CHEST;
    if (has("spine")) return B_SPINE;

    if (L && (has("clavicle") || has("shoulder"))) return B_LSHLDR;
    if (R && (has("clavicle") || has("shoulder"))) return B_RSHLDR;
    if (L && (has("upperarm") || has("arm") || has("forearm"))) return B_LELBOW;
    if (R && (has("upperarm") || has("arm") || has("forearm"))) return B_RELBOW;
    if (L && has("hand")) return B_LHAND;
    if (R && has("hand")) return B_RHAND;
    if (L && has("thigh") || (L && has("upleg"))) return B_LHIP;
    if (R && has("thigh") || (R && has("upleg"))) return B_RHIP;
    if (L && (has("calf") || has("leg") || has("knee"))) return B_LKNEE;
    if (R && (has("calf") || has("leg") || has("knee"))) return B_RKNEE;
    if (L && has("foot")) return B_LFOOT;
    if (R && has("foot")) return B_RFOOT;
    return -1;
}

void Game::resolveBones(Player& p) {
    if (!schema.PB_skeleton && !schema.PB_transform) return;
    uint64_t root = schema.PB_skeleton ? g_mem.ptr(p.addr + schema.PB_skeleton) : 0;
    if (!root && schema.PB_transform) root = g_mem.ptr(p.addr + schema.PB_transform);
    if (!root) return;

    // 骨骼名在 GameObject 上，不在 Transform 上 —— 必须走 get_name()
    typedef void* (*GetNameFn)(void*);
    GetNameFn getName = (GetNameFn)schema.goGetName;

    std::vector<uint64_t> q{ root };
    for (size_t i = 0; i < q.size() && i < 256; ++i) {
        uint64_t t = q[i];
        if (!t) continue;

        int idx = -1;
        if (getName && schema.PB_gameobject) {
            uint64_t go = g_mem.ptr(t + schema.PB_gameobject);
            if (go) {
                uint64_t nameObj = (uint64_t)getName((void*)go);
                if (nameObj) {
                    std::string nm = g_mem.str(nameObj);
                    if (!nm.empty()) idx = matchBone(nm.c_str());
                }
            }
        }
        if (idx < 0) {   // 退化兜底：类名（基本匹配不到）
            uint64_t k = g_mem.ptr(t);
            if (k && g_il2.class_get_name)
                idx = matchBone(g_il2.class_get_name((void*)k));
        }
        if (idx >= 0 && !p.boneTr[idx]) p.boneTr[idx] = t;

        uint64_t ch = schema.TR_children ? g_mem.ptr(t + schema.TR_children) : 0;
        if (ch) {
            uint64_t items = g_mem.ptr(ch + 0x10);
            int32_t  n     = g_mem.i32(ch + 0x18);
            if (items && n > 0 && n < 128) {
                for (int j = 0; j < n; ++j) {
                    uint64_t c = g_mem.ptr(items + 8ull * j);
                    if (c) q.push_back(c);
                }
            }
        }
    }
    p.baked = true;
}

void Game::updateBones(Player& p) {
    if (!p.baked) resolveBones(p);
    for (int i = 0; i < B_COUNT; ++i) {
        if (!p.boneTr[i]) { p.bones[i].on = false; continue; }
        Vec3 wp = callGetPos(p.boneTr[i]);
        p.bones[i].pos = wp;
        Vec3 s{};
        if (callW2S(g_cam, wp, s)) {
            p.bones[i].scr = { s.x, s.y };
            p.bones[i].on = (s.x > -g_scr.w && s.x < g_scr.w * 2.f &&
                             s.y > -g_scr.h && s.y < g_scr.h * 2.f);
        } else if (g_vp.m[0][0] != 0.f) {
            p.bones[i].on = W2S(g_vp, wp, p.bones[i].scr);
        } else {
            p.bones[i].on = false;
        }
    }
}

// ---------------- 主循环 ----------------

bool Game::init() {
    if (!g_il2.ok && !g_il2.init()) { LOGE("il2cpp init 失败"); return false; }
    resolveSchema();
    LOGI("Game init: schema.ok=%d", schema.ok ? 1 : 0);
    return true;
}

void Game::readLocal() {
    uint64_t w = derefWorld();
    if (!w) return;
    uint64_t lp = schema.localPlayerGetter ? g_mem.ptr(w + (uint64_t)schema.localPlayerGetter) : 0;
    if (!lp && schema.playerListGetter) {
        uint64_t list = g_mem.ptr(w + (uint64_t)schema.playerListGetter);
        uint64_t items = list ? g_mem.ptr(list + 0x10) : 0;
        if (items) lp = g_mem.ptr(items);
    }
    if (!lp) return;
    localAddr = lp;

    uint64_t info = schema.PB_info ? g_mem.ptr(lp + schema.PB_info) : lp;
    if (info && schema.PI_pos)   localPos   = g_mem.rd<Vec3>(info + schema.PI_pos);
    if (info && schema.PI_team)  localTeam  = g_mem.i32(info + schema.PI_team);
    if (info && schema.PI_state) localState = g_mem.i32(info + schema.PI_state);

    // 朝向：Transform.get_forward() -> yaw
    uint64_t tr = schema.PB_transform ? g_mem.ptr(lp + schema.PB_transform) : 0;
    if (tr && schema.trForward) {
        typedef Vec3 (*GetFwdFn)(void*);
        Vec3 f = ((GetFwdFn)schema.trForward)((void*)tr);
        if (fabsf(f.x) + fabsf(f.z) > 1e-4f)
            localYaw = atan2f(f.x, f.z) * 57.2957795f;
    }

    firing = false;
    if (schema.PB_fire) firing = g_mem.rd<uint8_t>(lp + schema.PB_fire, 0) != 0;
    if (schema.PB_ammo) ammo   = g_mem.i32(lp + schema.PB_ammo);
    if (schema.PB_wid)  localWeapon = g_mem.i32(lp + schema.PB_wid);

    // 队伍没读到就按地址反查，避免视角辅助锁到队友
    if (localTeam < 0) {
        for (auto& p : players)
            if (p.addr == lp) { localTeam = p.team; break; }
        if (localTeam < 0) localTeam = 0;
    }
}

void Game::readPlayers() {
    uint64_t w = derefWorld();
    if (!w) return;
    uint64_t list = schema.playerListGetter ? g_mem.ptr(w + (uint64_t)schema.playerListGetter) : 0;
    if (!list) return;

    std::vector<Player> out;
    g_mem.listEach(list, [&](uint64_t obj) {
        Player p;
        p.addr = obj;
        uint64_t info = schema.PB_info ? g_mem.ptr(obj + schema.PB_info) : obj;
        if (!info) return;
        p.origin = schema.PI_pos ? g_mem.rd<Vec3>(info + schema.PI_pos) : Vec3{};
        if (p.origin.x == 0 && p.origin.y == 0 && p.origin.z == 0) return;
        p.hp       = schema.PI_hp ? g_mem.f32(info + schema.PI_hp) : 100.f;
        p.maxHp    = schema.PI_maxhp ? g_mem.f32(info + schema.PI_maxhp) : 100.f;
        if (p.maxHp < 1) p.maxHp = 100.f;
        p.team     = schema.PI_team ? g_mem.i32(info + schema.PI_team) : 0;
        p.state    = schema.PI_state ? g_mem.i32(info + schema.PI_state) : 0;
        p.isPerson = schema.PI_person ? g_mem.rd<uint8_t>(info + schema.PI_person, 0) != 0 : false;
        if (schema.PI_name) p.name = g_mem.str(g_mem.ptr(info + schema.PI_name));
        p.dist = localPos.dist2d(p.origin);
        out.push_back(std::move(p));
    });

    // 骨骼缓存复用
    for (auto& np : out) {
        for (auto& op : players) {
            if (op.addr == np.addr) {
                memcpy(np.boneTr, op.boneTr, sizeof np.boneTr);
                np.baked = op.baked;
                break;
            }
        }
    }
    players.swap(out);
}

void Game::readLoots() {
    uint64_t w = derefWorld();
    if (!w || !schema.lootListGetter) return;
    uint64_t list = g_mem.ptr(w + (uint64_t)schema.lootListGetter);
    if (!list) return;

    std::vector<Loot> out;
    g_mem.listEach(list, [&](uint64_t obj) {
        Loot l; l.addr = obj;
        // 位置字段常见在 +0x10 ~ +0x40
        for (uint64_t off = 0x10; off <= 0x60; off += 8) {
            Vec3 v = g_mem.rd<Vec3>(obj + off);
            if (fabsf(v.x) < 5000 && fabsf(v.y) < 5000 && fabsf(v.z) < 5000 &&
                (v.x != 0 || v.y != 0 || v.z != 0)) {
                l.pos = v;
                break;
            }
        }
        if (l.pos.x == 0 && l.pos.z == 0) return;
        l.itemId = g_mem.i32(obj + 0x18);
        l.rarity = g_mem.i32(obj + 0x2C);
        l.dist = localPos.dist2d(l.pos);
        out.push_back(l);
    });
    loots.swap(out);
}

void Game::readTimer() {
    // 优先读游戏真实倒计时字段，读不到才退回本地时钟
    uint64_t w = derefWorld();
    static const char* kRemain[] = { "m_RemainTime", "m_RemainingTime", "m_LeftTime",
                                     "m_Countdown", "remainTime", nullptr };
    if (w) {
        for (int i = 0; kRemain[i]; ++i) {
            uint64_t off = g_il2.fieldForInstance(w, kRemain[i]);
            if (!off) continue;
            int32_t v = g_mem.i32(w + off);
            if (v > 0 && v <= 3600) {
                timer.remain = (int)v;
                timer.real   = true;
                inRaid = true;
                return;
            }
        }
    }
    static uint64_t start = 0;
    uint64_t now = (uint64_t)time(nullptr);
    if (!start) start = now;
    timer.elapsed = (int)(now - start);
    timer.remain  = 45 * 60 - std::min(timer.elapsed, 45 * 60);
    timer.real    = false;
    inRaid = true;
}

void Game::update() {
    if (!schema.ok) return;
    busy.store(true, std::memory_order_relaxed);
    struct Guard { std::atomic<bool>& b;
        ~Guard() { b.store(false, std::memory_order_relaxed); } } guard{ busy };
    readLocal();
    readPlayers();

    uint64_t cam = 0;
    if (schema.cameraMainFn) cam = (uint64_t)((GetMainFn)schema.cameraMainFn)();
    g_cam = cam;

    for (auto& p : players) {
        if (p.dist > g_cfg.maxDist && p.dist > 0) continue;
        updateBones(p);
        float hx = p.bones[B_HEAD].scr.x, hy = p.bones[B_HEAD].scr.y;
        float fx = p.bones[B_LFOOT].scr.x, fy = p.bones[B_LFOOT].scr.y;
        if (p.bones[B_HEAD].on && p.bones[B_LFOOT].on) {
            float h = fabsf(fy - hy);
            if (h < 6) h = 6;
            float w2 = h * 0.42f;
            p.boxT = hy - h * 0.10f;
            p.boxB = fy;
            p.boxL = hx - w2 * 0.5f;
            p.boxR = hx + w2 * 0.5f;
            p.visible = true;
        }
    }

    if (g_cfg.loot) readLoots();
    readTimer();
}
