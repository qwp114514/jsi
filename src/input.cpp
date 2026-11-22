// input.cpp — 触摸屏事件读取（给菜单用）
#include "input.hpp"
#include "resolver.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <chrono>

TouchReader g_touchReader;

static bool testBit(const unsigned long* bits, int bit) {
    return (bits[bit / (8 * sizeof(long))] >> (bit % (8 * sizeof(long)))) & 1UL;
}

bool TouchReader::findTouchDevice() {
    DIR* d = opendir("/dev/input");
    if (!d) return false;
    dirent* e;
    while ((e = readdir(d))) {
        if (strncmp(e->d_name, "event", 5) != 0) continue;
        char p[64];
        snprintf(p, sizeof p, "/dev/input/%s", e->d_name);
        int fd = open(p, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) continue;

        unsigned long absBits[8] = {0};
        unsigned long propBits[4] = {0};
        ioctl(fd, EVIOCGBIT(EV_ABS, sizeof absBits), absBits);
        ioctl(fd, EVIOCGPROP(sizeof propBits), propBits);

        bool hasMt = testBit(absBits, ABS_MT_POSITION_X) &&
                     testBit(absBits, ABS_MT_POSITION_Y);
        bool direct = testBit(propBits, INPUT_PROP_DIRECT);

        if (hasMt && direct) {
            input_absinfo ai{};
            if (ioctl(fd, EVIOCGABS(ABS_MT_POSITION_X), &ai) == 0)
                maxX_ = ai.maximum > 0 ? ai.maximum : maxX_;
            if (ioctl(fd, EVIOCGABS(ABS_MT_POSITION_Y), &ai) == 0)
                maxY_ = ai.maximum > 0 ? ai.maximum : maxY_;
            fd_ = fd;
            LOGI("input: touch device %s  range %dx%d", p, maxX_, maxY_);
            closedir(d);
            return true;
        }
        close(fd);
    }
    closedir(d);
    return false;
}

bool TouchReader::start() {
    if (fd_ >= 0) return true;
    return findTouchDevice();
}

void TouchReader::stop() {
    if (fd_ >= 0) { close(fd_); fd_ = -1; }
}

TouchReader::Point TouchReader::poll() {
    Point out{ down_, x_, y_, fingers_ };
    if (fd_ < 0) return out;

    input_event ev[64];
    ssize_t n = read(fd_, ev, sizeof ev);
    if (n <= 0) return out;

    int count = (int)(n / sizeof(input_event));
    int curSlot = 0;
    for (int i = 0; i < count; ++i) {
        auto& e = ev[i];
        if (e.type == EV_ABS) {
            if (e.code == ABS_MT_SLOT) curSlot = e.value & 0xF;
            else if (e.code == ABS_MT_POSITION_X || e.code == ABS_X) x_ = e.value;
            else if (e.code == ABS_MT_POSITION_Y || e.code == ABS_Y) y_ = e.value;
            else if (e.code == ABS_MT_TRACKING_ID) {
                slots_[curSlot] = (e.value >= 0);
                if (e.value >= 0) down_ = true;
            }
        } else if (e.type == EV_KEY && e.code == BTN_TOUCH) {
            down_ = e.value != 0;
            if (!down_) memset(slots_, 0, sizeof slots_);
        }
    }
    fingers_ = 0;
    for (int i = 0; i < 10; ++i) if (slots_[i]) ++fingers_;
    out = { down_, x_, y_, fingers_ };
    return out;
}
