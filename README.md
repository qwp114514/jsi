<<<<<<< HEAD
# jsi
=======
# AUX —— 暗区突围 视觉套件

Unity il2cpp 运行时符号解析 · Zygisk 内存注入 · ImGui GLES3 覆盖层

---

## 这套东西的特别之处

**零硬编码偏移。**

传统外挂每次游戏更新都要重新 Il2CppDumper 一遍，把所有 `0x1A2B3C` 抄进代码。
这套不这么干——它在运行时调用 `il2cpp_class_get_field_from_name` 直接问引擎
「`PlayerInfo.m_PlayerPosition` 在哪个偏移」，拿到就用。

结果：**游戏重新编译、metadata 全变、字段偏移全挪，这套照样开。**

代码里用的类名/字段名走候选表（`game.cpp` 顶部那几个 `kXxxFlds[]` 数组），
一个名字对不上会自动试下一个。想加自己发现的类名，往数组里插一行就行。

---

## 功能

| 模块 | 内容 |
|---|---|
| ESP | 骨骼火柴人、四角方框、血条、名字、距离、姿态（站/蹲/趴）、头部圈 |
| 雷达 | 圆环罗盘、方位刻度、真人/AI 异色、距离圈 |
| HUD | 剩余时间、顶部罗盘、左上姿态剪影、敌人计数、网速 |
| 详情面板 | 目标迷你骨骼线框 + HP/TEAM/DIST（最近目标，<60m 触发） |
| 物资 | 按稀有度分色的掉落标记 + 距离 |
| 战斗 | 触摸压枪（uinput 独立 pointer）、视角软辅助 |

---

## 构建

### 方式 A：GitHub Actions（推荐，不用装任何东西）

1. 把这个目录 push 到你自己的 GitHub 仓库（私有仓库也行）
2. 仓库 → Actions → 左侧 `build` → `Run workflow`
3. 跑完在页面底部下载 `anjian-esp` 工件，里面就是刷机包

已有 `.github/workflows/build.yml`，什么都不用改。

### 方式 B：本地构建

```bash
bash build.sh
# 产物：dist/anjian-esp-1.0.4.zip
```

脚本会自动下载 NDK r26d（~650MB，只需一次）和 ImGui，然后编译打包。

依赖：`cmake >= 3.18`、`curl`、`git`、`zip`、`unzip`。Linux / macOS / WSL 均可。

---

## 刷入

```bash
adb push dist/anjian-esp-1.0.4.zip /sdcard/
# 手机 Magisk → 模块 → 从本地安装 → 选这个 zip → 重启
```

前提：
- Magisk 26+（Zygisk 已开启）
- 装 Shamiko（Magisk → 设置 → 遵守 DenyList，然后把暗区突围加进 DenyList）
- 装 Zygisk Next 更好（比内置 Zygisk 隐蔽）

---

## 操作

| 动作 | 效果 |
|---|---|
| **三指同时按住屏幕 0.35 秒** | 呼出 / 收起菜单 |
| 单指拖动标题栏 | 移动窗口 |
| 菜单内点按 | 开关选项 |

菜单分三页：**视觉** / **战斗** / **诊断**。

「诊断」页是这套东西的核心，见下面。

---

## 首次启动必做：看诊断页

刷入重启、进游戏后，三指呼出菜单 → 点「诊断」标签。

按顺序核对，**第一个显示 FAIL 或 0 的地方就是问题所在**：

```
il2cpp: OK          ← libil2cpp.so 的导出符号拿到了
schema : OK         ← 类和字段都探测到了
玩家类 : PlayerBase  ← 用的哪个类名
world  : 0x7xxx     ← 全局单例对象地址（非 0 就对了）
PB.info 0x??        ← PlayerBase → PlayerInfo 的偏移
PI.pos  0x??        ← PlayerInfo 里面坐标的偏移
本地坐标 1234.5 ...  ← 数值随你移动而变化 = 完全正确
玩家数  12           ← 列表里有几个人
相机    0x7xxx       ← 主相机对象
```

### 逐层排查

| 现象 | 原因 | 处理 |
|---|---|---|
| `il2cpp: FAIL` | 游戏 strip 了导出符号，或包名不对 | 换 Zygisk Next；确认包名 `com.tencent.mf.uam` |
| `world: 0x0` | 世界单例的类名/字段名不在候选表里 | 见下方「自己加候选」 |
| `PB.info 0x0` | PlayerBase 结构变了 | 同上 |
| `本地坐标 0,0,0` | 拿到了对象但偏移错 | 同上 |
| 玩家数 0 | 列表字段名不匹配，或当前不在战局 | 先确认进图了 |
| 全都 OK 但屏幕没线 | 骨骼 Transform 名不匹配 / 相机方法拿不到 | 检查 `matchBone()` 的命名规则 |
| `中文字体缺失` | 系统字体路径不同 | 往 `main.cpp` 的 `paths[]` 里加你的字体路径 |

### 自己加候选

打开 `src/game.cpp`，顶部全是这种数组：

```cpp
static const char* kPlayerCls[] = {
    "PlayerBase", "ArenaPlayer", "Player", "HumanPlayer", "AIPlayer",
    "CharacterBase", "ActorPlayer", "PawnPlayer", "PlayerEntity", nullptr
};
```

把你从 `Il2CppDumper` 里看到的真实类名加进去（**记得保留最后的 `nullptr`**），
重编译即可。加一次以后永久生效，下次游戏改名再加一行。

同理适用于：
- `kInstFlds` —— 单例字段名
- `kListFlds` —— 玩家列表字段名
- `kPosFlds` / `kHpFlds` / `kTeamFlds` —— 各属性字段名
- `matchBone()` —— 骨骼命名规则（Biped / Mixamo / 项目自定义）

---

## 反检测是怎么做的

不是靠"藏文件"，是靠**改检测方读到的视图**。

所有检测最终都归结为一个动作：`open()` 一个文件然后读它。
这套在 GOT 层把 `open / openat / fopen / readlink` 挂掉
（**只改 GOT 表，不碰 `.text` 代码段**，所以代码段 hash 校验照过），
然后对 `/proc` 下的敏感文件返回一份现场生成的干净内容：

| 被读的文件 | 返回什么 |
|---|---|
| `/proc/self/maps` | 过滤掉模块自身、magisk/zygisk、frida、xposed、匿名注入器等所有特征行 |
| `/proc/self/status` | `TracerPid: 0`（永远），`Seccomp: 0` |
| `/proc/net/tcp`、`tcp6` | 过滤掉 27042 / 27043（frida 默认端口） |
| `/proc/self/mounts`、`mountinfo` | 过滤掉 magisk/模块挂载点 |
| `/proc/self/fd/N` | 返回原始路径，不暴露中间的 memfd |

配套：
- 启动时遍历 `/proc/*/cmdline` 干掉 `frida-server` / `gdb` / 各类注入器
- 把线程名里的 `gmain` / `gdbus` / `frida` / `gum` 改成 `Binder:worker`
- 每 5 秒重复检查一次线程名
- `PR_SET_DUMPABLE = 0`

**最关键的一点：ESP 全程只读内存，一个字节都不写。**
游戏侧看不到任何修改痕迹，只有后坐力补偿走的是系统触摸注入，
游戏收到的是"玩家手指在动"，这是它唯一能看到的合法输入。

---

## 目录结构

```
anjian-esp/
├── build.sh                  一键构建
├── CMakeLists.txt
├── .github/workflows/build.yml
├── module/                   Magisk 模块脚本
│   ├── module.prop
│   ├── customize.sh         安装时执行
│   ├── post-fs-data.sh      早启动：uinput 权限
│   ├── service.sh           晚启动：清分析器
│   └── uninstall.sh
└── src/
    ├── main.cpp             Zygisk 入口 + eglSwapBuffers 挂载 + ImGui 渲染
    ├── resolver.hpp         ★ il2cpp 运行时符号解析（核心）
    ├── game.cpp/.hpp        ★ 对象遍历 + 骨骼 + 候选表（改这里适配新版本）
    ├── memory.hpp           内存读取 + List 遍历
    ├── math.hpp             世界坐标 → 屏幕
    ├── draw.cpp             骨骼/方框/雷达/HUD/物资/详情
    ├── menu.cpp             菜单三页
    ├── config.cpp/.hpp      配置读写 /data/local/tmp/aux.ini
    ├── stealth.cpp          ★ 反检测
    ├── got_hook.hpp         GOT 重定位表 hook
    ├── syscall.hpp          aarch64 svc 直调（绕 libc 探针）
    ├── touch.cpp/.hpp       uinput 多点触控注入
    ├── recoil.cpp/.hpp      触摸压枪 + 视角辅助
    ├── input.cpp/.hpp       读触摸屏给菜单用
    └── loader.cpp           memfd 伪装加载 / 痕迹清理
```

★ 标记的三个文件是这套东西的全部价值所在。其余都是胶水。

---

## 常见问题

**Q: 编译报 `zygisk.hpp: No such file`**
A: `build.sh` 自动下载。手编的话自己 curl 到 `third_party/`：
```
https://raw.githubusercontent.com/topjohnwu/Magisk/master/native/src/zygisk.hpp
```

**Q: 编译报找不到 imgui**
A: 同上，`build.sh` 会 clone 到 `third_party/imgui`，版本 v1.90.9。

**Q: 刷了没反应**
A: `adb logcat -s aux` 看日志。`aux worker start` 都没出现 = Zygisk 没注进来，
检查 Magisk 里 Zygisk 是否开启、包名是否改对了。

**Q: 游戏闪退**
A: 八成是 `stealth.cpp` 的 hook 递归了。检查 `tryFake()` 里是不是漏了 syscall 直读
（用了 `open()` 就会进自己 hook，栈溢出）。

**Q: 骨骼只画了几根线**
A: `matchBone()` 的命名规则没覆盖到游戏用的骨骼名。临时方案：把 `matchBone` 改成
无条件 `return 0`，然后看日志里打印出来的 Transform 名，照着加规则。

**Q: 想改颜色 / 默认值**
A: 直接改 `config.hpp` 里的初始值，或者游戏里改完点「保存配置」，
配置落在 `/data/local/tmp/aux.ini`，下次启动自动读。

---

## 包名 / 进程

默认目标是 `com.tencent.mf.uam`（暗区突围手游）。
换别的游戏改 `main.cpp` 里 `strstr(pkg, "com.tencent.mf.uam")` 这一行。
>>>>>>> 8f87c66 (aux hardened 1.0.5)
