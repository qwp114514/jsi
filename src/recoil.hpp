#pragma once

class Recoil {
public:
    void start();
    void stop();
    void tick(float dt);      // 每帧调用

private:
    float offX_ = 0, offY_ = 0;   // 累计补偿量（像素）
    float curX_ = 0, curY_ = 0;
    bool  active_ = false;
    float idle_ = 0;
    int   slot_ = 7;
};

extern Recoil g_recoil;
