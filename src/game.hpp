#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <atomic>
#include "memory.hpp"
#include "math.hpp"

enum BoneIdx {
    B_HEAD, B_NECK, B_CHEST, B_SPINE, B_PELVIS,
    B_LSHLDR, B_LELBOW, B_LHAND,
    B_RSHLDR, B_RELBOW, B_RHAND,
    B_LHIP, B_LKNEE, B_LFOOT,
    B_RHIP, B_RKNEE, B_RFOOT,
    B_COUNT
};

struct Bone {
    Vec3 pos{};
    Vec2 scr{};
    bool on = false;
};

struct Player {
    uint64_t addr = 0;
    Vec3  origin{};
    float hp = 0, maxHp = 100;
    int   team = 0;
    int   state = 0;
    bool  isPerson = false;
    std::string name;
    float dist = 0;

    Bone  bones[B_COUNT];
    bool  visible = false;
    float boxL = 0, boxT = 0, boxR = 0, boxB = 0;
    Vec2  headScr{}, footScr{};

    // 骨骼 Transform 缓存
    uint64_t boneTr[B_COUNT] = {0};
    bool     baked = false;
};

struct Loot {
    uint64_t addr = 0;
    Vec3 pos{};
    int  itemId = 0;
    int  rarity = 0;
    float dist = 0;
    Vec2 scr{};
    bool on = false;
};

struct TimerInfo {
    int  remain  = 0;   // 秒
    int  elapsed = 0;
    bool real    = false; // 是否读到游戏真实倒计时
};

extern uint64_t g_cam;
extern Mat4 g_vp;
class Game {
public:
    bool init();

    void update();          // 每帧
    void updateBones(Player& p);

    bool w2s(const Vec3& w, Vec3& out) { return callW2S(g_cam, w, out); }

    bool callW2S(uint64_t cam, const Vec3& w, Vec3& out);

    std::atomic<bool> busy{ false };   // 更新中标志，防止渲染线程读到半成品

    std::vector<Player> players;
    std::vector<Loot>   loots;
    TimerInfo timer;

    // 本地状态
    Vec3  localPos{};
    float localYaw = 0;
    int   localTeam = -1;
    int   localState = 0;
    int   localWeapon = 0;
    int   ping = 0;
    bool  inRaid = false;
    uint64_t localAddr = 0;    // 本地玩家对象地址（队友过滤用）
    bool  firing = false;      // 开火状态（压枪触发源）
    int   ammo = 0;

    // 解析结果
    struct Schema {
        bool  ok = false;
        const char* playerCls = nullptr;
        uint64_t PB_info = 0, PB_transform = 0, PB_skeleton = 0, PB_weapon = 0;
        uint64_t PI_pos = 0, PI_hp = 0, PI_maxhp = 0, PI_team = 0,
                 PI_name = 0, PI_state = 0, PI_person = 0;
        uint64_t PB_fire = 0, PB_ammo = 0, PB_wid = 0;
        uint64_t TR_children = 0, TR_father = 0;
        void*  worldGetter = nullptr;    // 静态字段地址
        void*  playerListGetter = nullptr;
        void*  localPlayerGetter = nullptr;
        void*  lootListGetter = nullptr;
        void*  cameraMainFn = nullptr;   // Camera.get_main()
        void*  w2spFn = nullptr;         // Camera.WorldToScreenPoint
        void*  transformGetPos = nullptr;// Transform.get_position()
        // ---- 加固补丁新增 ----
        uint64_t PB_gameobject = 0;      // Transform.m_GameObject
        void*  goGetName       = nullptr;// GameObject.get_name()
        void*  trForward       = nullptr;// Transform.get_forward()
    } schema;

private:
    void resolveSchema();
    void resolveBones(Player& p);
    uint64_t derefWorld();
    Vec3 callGetPos(uint64_t transform);
    void readLocal();
    void readPlayers();
    void readLoots();
    void readTimer();
};

extern Game g_game;
