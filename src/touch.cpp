// touch.cpp — 触摸注入（加固版）
// 原件的问题（这是整套里最容易导致封号的一处）：
//   uinput 设备名写死 "aux-touch"，vendor 0x2710 / product 0x1501。
//   /proc/bus/input/devices 里会多出一台 vendor=2710 product=1501 name=aux-touch
//   的虚拟触屏 —— 任何一家手游反作弊枚举一遍就命中，跟外挂功能有没有开无关。
//
// 加固：
//   1. 从 /proc/bus/input/devices 里读出本机真实触摸屏的 Name/Vendor/Product/Version，
//      原样复用，并 UI_SET_PHYS 伪造物理路径，让新设备长得和内置触屏一模一样。
//   2. 优先走「直写已有触摸屏 event 节点」模式（initDevice），根本不创建新设备，
//      /proc/bus/input/devices 完全不变。这是最隐蔽的一条路径。
//   3. 修掉原件 tracking id 的坑：同一 slot 未 up 就再次 down，
//      内核会丢弃事件流，表现为压枪突然失效。
#include "touch.hpp"
#include "resolver.hpp"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <cerrno>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/uinput.h>
#include <linux/input.h>

TouchInj g_touch;

static int nextTid = 3000;

struct RealDev {
    char name[80]  = "NVTCapacitiveTouchScreen";
    char phys[80]  = "usb-xhci-hcd.0.auto-1/input0";
    int  vendor    = 0x0000;
    int  product   = 0x0000;
    int  version   = 0x0100;
};

// 从 /proc/bus/input/devices 里挑「最像触摸屏」的那一块，原样复用它的描述
static int scoreName(const char* n) {
    int s = 0;
    if (!n) return 0;
    if (strstr(n, "ouch") || strstr(n, "TOUCH")) s += 3;
    if (strstr(n, "TP") || strstr(n, "tp_") || strstr(n, "Ts")) s += 2;
    if (strstr(n, "Goodix") || strstr(n, "Synaptics") || strstr(n, "FocalTech") ||
        strstr(n, "Novatek") || strstr(n, "NVT") || strstr(n, "Atmel")) s += 3;
    if (strstr(n, "key") || strstr(n, "Key") || strstr(n, "pwr") || strstr(n, "gpio")) s -= 5;
    return s;
}

static void readRealDev(RealDev& d) {
    FILE* f = fopen("/proc/bus/input/devices", "r");
    if (!f) return;
    char line[256];
    RealDev cur; int curScore = -999; bool have = false;

    auto commit = [&]() {
        if (!have) return;
        int sc = scoreName(cur.name);
        if (sc > curScore) { curScore = sc; d.name[0] = 0; strncpy(d.name, cur.name, sizeof d.name - 1);
                             strncpy(d.phys, cur.phys, sizeof d.phys - 1);
                             d.vendor = cur.vendor; d.product = cur.product; d.version = cur.version; }
        have = false;
        cur = RealDev{};
    };

    while (fgets(line, sizeof line, f)) {
        if (line[0] == 'I' && line[1] == ':') {
            commit();
            int v = 0, p = 0, ver = 0;
            if (sscanf(line, "I: Bus=%*x Vendor=%x Product=%x Version=%x", &v, &p, &ver) == 3) {
                cur.vendor = v; cur.product = p; cur.version = ver;
            }
        } else if (line[0] == 'N' && line[1] == ':') {
            char* q = strchr(line, '"');
            if (q) { char* e = strrchr(q + 1, '"'); if (e) { *e = 0; strncpy(cur.name, q + 1, sizeof cur.name - 1); have = true; } }
        } else if (line[0] == 'P' && line[1] == ':') {
            char* q = strchr(line, '"');
            if (q) { char* e = strrchr(q + 1, '"'); if (e) { *e = 0; strncpy(cur.phys, q + 1, sizeof cur.phys - 1); } }
        }
    }
    commit();
    fclose(f);
}

bool TouchInj::init(int screenW, int screenH) {
    if (fd_ >= 0) return true;
    w_ = screenW > 0 ? screenW : 1440;
    h_ = screenH > 0 ? screenH : 3200;

    RealDev dev; readRealDev(dev);
    LOGI("touch: mimic name=\"%s\" vendor=%04x product=%04x", dev.name, dev.vendor, dev.product);

    fd_ = open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd_ < 0) fd_ = open("/dev/input/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd_ < 0) { LOGE("touch: /dev/uinput 打不开 (%s)", strerror(errno)); return false; }

    ioctl(fd_, UI_SET_EVBIT, EV_SYN);
    ioctl(fd_, UI_SET_EVBIT, EV_KEY);
    ioctl(fd_, UI_SET_EVBIT, EV_ABS);
    ioctl(fd_, UI_SET_KEYBIT, BTN_TOUCH);
    ioctl(fd_, UI_SET_PROPBIT, INPUT_PROP_DIRECT);
    ioctl(fd_, UI_SET_ABSBIT, ABS_MT_SLOT);
    ioctl(fd_, UI_SET_ABSBIT, ABS_MT_TRACKING_ID);
    ioctl(fd_, UI_SET_ABSBIT, ABS_MT_POSITION_X);
    ioctl(fd_, UI_SET_ABSBIT, ABS_MT_POSITION_Y);
    ioctl(fd_, UI_SET_ABSBIT, ABS_MT_PRESSURE);
    ioctl(fd_, UI_SET_ABSBIT, ABS_MT_TOUCH_MAJOR);

    auto absSetup = [&](int code, int mn, int mx) {
        uinput_abs_setup a{};
        a.code = code;
        a.absinfo.minimum = mn; a.absinfo.maximum = mx;
        ioctl(fd_, UI_ABS_SETUP, &a);
    };
    absSetup(ABS_MT_SLOT, 0, 9);
    absSetup(ABS_MT_TRACKING_ID, 0, 65535);
    absSetup(ABS_MT_POSITION_X, 0, w_ - 1);
    absSetup(ABS_MT_POSITION_Y, 0, h_ - 1);
    absSetup(ABS_MT_PRESSURE, 0, 255);
    absSetup(ABS_MT_TOUCH_MAJOR, 0, 15);

    uinput_setup us{};
    snprintf(us.name, sizeof us.name, "%s", dev.name);   // 复用真实名字
    us.id.bustype = BUS_I2C;                             // 内置触屏常见总线
    us.id.vendor  = (uint16_t)dev.vendor;
    us.id.product = (uint16_t)dev.product;
    us.id.version = (uint16_t)dev.version;

    if (ioctl(fd_, UI_DEV_SETUP, &us) < 0) {
        LOGE("touch: UI_DEV_SETUP 失败 %s", strerror(errno));
        close(fd_); fd_ = -1; return false;
    }
#ifdef UI_SET_PHYS
    ioctl(fd_, UI_SET_PHYS, dev.phys);                   // 伪造物理路径
#endif
    if (ioctl(fd_, UI_DEV_CREATE) < 0) {
        LOGE("touch: UI_DEV_CREATE 失败 %s", strerror(errno));
        close(fd_); fd_ = -1; return false;
    }
    usleep(150000);
    LOGI("touch: uinput ready %dx%d", w_, h_);
    return true;
}

// 模式 2：直接写已有触摸屏节点，不产生任何新设备（最隐蔽）
bool TouchInj::initDevice(const char* evdevPath, int screenW, int screenH) {
    if (fd_ >= 0) return true;
    w_ = screenW > 0 ? screenW : 1440;
    h_ = screenH > 0 ? screenH : 3200;

    fd_ = open(evdevPath, O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd_ < 0) { LOGE("touch: %s 不可写 (%s)", evdevPath, strerror(errno)); return false; }

    input_absinfo ai{};
    if (ioctl(fd_, EVIOCGABS(ABS_MT_POSITION_X), &ai) == 0 && ai.maximum > 0) w_ = ai.maximum + 1;
    if (ioctl(fd_, EVIOCGABS(ABS_MT_POSITION_Y), &ai) == 0 && ai.maximum > 0) h_ = ai.maximum + 1;

    direct_ = true;
    slot_ = 9;                       // 真实手指基本不用最后一槽
    LOGI("touch: direct on %s %dx%d", evdevPath, w_, h_);
    return true;
}

void TouchInj::emit(int type, int code, int32_t val) {
    if (fd_ < 0) return;
    input_event ev{};
    ev.type = (uint16_t)type;
    ev.code = (uint16_t)code;
    ev.value = val;
    if (write(fd_, &ev, sizeof ev) != (ssize_t)sizeof ev) {
        usleep(300);
        write(fd_, &ev, sizeof ev);
    }
}

void TouchInj::sync() { emit(EV_SYN, SYN_REPORT, 0); }

void TouchInj::down(int slot, int32_t x, int32_t y) {
    if (fd_ < 0 || slot < 0 || slot > 9) return;
    if (x < 0) x = 0; if (x >= w_) x = w_ - 1;
    if (y < 0) y = 0; if (y >= h_) y = h_ - 1;

    // 关键修复：同一 slot 还按着就先抬起来，否则 tracking id 跳变会被内核丢弃
    if (used_[slot]) up(slot);

    emit(EV_ABS, ABS_MT_SLOT, slot);
    tid_[slot] = (nextTid++ % 50000) + 1000;
    emit(EV_ABS, ABS_MT_TRACKING_ID, tid_[slot]);
    emit(EV_ABS, ABS_MT_POSITION_X, x);
    emit(EV_ABS, ABS_MT_POSITION_Y, y);
    emit(EV_ABS, ABS_MT_PRESSURE, 120);
    emit(EV_ABS, ABS_MT_TOUCH_MAJOR, 8);
    emit(EV_KEY, BTN_TOUCH, 1);
    sync();

    used_[slot] = true;
    lastX_[slot] = x; lastY_[slot] = y;
}

void TouchInj::move(int slot, int32_t x, int32_t y) {
    if (fd_ < 0 || slot < 0 || slot > 9 || !used_[slot]) return;
    if (x < 0) x = 0; if (x >= w_) x = w_ - 1;
    if (y < 0) y = 0; if (y >= h_) y = h_ - 1;
    if (x == lastX_[slot] && y == lastY_[slot]) return;   // 去掉无效事件

    emit(EV_ABS, ABS_MT_SLOT, slot);
    emit(EV_ABS, ABS_MT_POSITION_X, x);
    emit(EV_ABS, ABS_MT_POSITION_Y, y);
    sync();
    lastX_[slot] = x; lastY_[slot] = y;
}

void TouchInj::up(int slot) {
    if (fd_ < 0 || slot < 0 || slot > 9 || !used_[slot]) return;
    emit(EV_ABS, ABS_MT_SLOT, slot);
    emit(EV_ABS, ABS_MT_TRACKING_ID, -1);
    emit(EV_KEY, BTN_TOUCH, 0);
    sync();
    used_[slot] = false;
}

void TouchInj::shutdown() {
    if (fd_ < 0) return;
    for (int i = 0; i < 10; ++i) if (used_[i]) up(i);
    if (!direct_) ioctl(fd_, UI_DEV_DESTROY);
    close(fd_);
    fd_ = -1;
}
