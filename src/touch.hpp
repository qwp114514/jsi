#pragma once
#include <cstdint>

class TouchInj {
public:
    bool init(int screenW, int screenH);                              // uinput（伪装成真实触屏）
    bool initDevice(const char* evdevPath, int w, int h);             // 直写已有 event 节点
    void shutdown();

    void down(int slot, int32_t x, int32_t y);
    void move(int slot, int32_t x, int32_t y);
    void up(int slot);

    bool ok() const { return fd_ >= 0; }
    bool isDirect() const { return direct_; }

private:
    void emit(int type, int code, int32_t val);
    void sync();
    int  fd_ = -1;
    int  w_ = 1440, h_ = 3200;
    int  slot_ = 9;
    bool direct_ = false;
    bool used_[16] = { false };
    int  tid_[16]  = { 0 };
    int32_t lastX_[16] = { 0 }, lastY_[16] = { 0 };
};

extern TouchInj g_touch;
