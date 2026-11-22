#pragma once
#include <linux/input.h>
#include <cstdint>

// 直接读触摸屏设备节点，给 ImGui 菜单喂输入
class TouchReader {
public:
    bool start();
    void stop();

    // 返回手指 0 的状态
    struct Point { bool down = false; int32_t x = 0, y = 0; int fingers = 0; };
    Point poll();

    int screenW() const { return maxX_; }
    int screenH() const { return maxY_; }

private:
    int  fd_ = -1;
    int  tw_ = 1080, th_ = 2400;
    int  maxX_ = 32767, maxY_ = 32767;
    bool down_ = false;
    int  fingers_ = 0;
    bool slots_[10] = { false };
    int32_t x_ = 0, y_ = 0;
    bool findTouchDevice();
};

extern TouchReader g_touchReader;
