#include "imgui.h"
// math.hpp
#pragma once
#include <cmath>
#include <algorithm>

struct Vec2 { float x, y; };
struct Vec3 {
    float x, y, z;
    Vec3 operator-(const Vec3& o) const { return { x - o.x, y - o.y, z - o.z }; }
    Vec3 operator+(const Vec3& o) const { return { x + o.x, y + o.y, z + o.z }; }
    Vec3 operator*(float s) const { return { x * s, y * s, z * s }; }
    float len() const { return sqrtf(x * x + y * y + z * z); }
    float dist2d(const Vec3& o) const {
        float dx = x - o.x, dz = z - o.z;
        return sqrtf(dx * dx + dz * dz);
    }
};

struct Mat4 { float m[4][4]; };

// Unity 矩阵：列主序。VP = proj * view
inline Mat4 mul(const Mat4& a, const Mat4& b) {
    Mat4 r{};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            float s = 0;
            for (int k = 0; k < 4; ++k) s += a.m[k][i] * b.m[j][k];
            r.m[j][i] = s;
        }
    return r;
}

struct Screen { int w, h; };
inline Screen g_scr{ 1280, 576 };

inline bool W2S(const Mat4& vp, const Vec3& w, Vec2& out) {
    float cx = w.x * vp.m[0][0] + w.y * vp.m[1][0] + w.z * vp.m[2][0] + vp.m[3][0];
    float cy = w.x * vp.m[0][1] + w.y * vp.m[1][1] + w.z * vp.m[2][1] + vp.m[3][1];
    float cw = w.x * vp.m[0][3] + w.y * vp.m[1][3] + w.z * vp.m[2][3] + vp.m[3][3];
    if (cw < 0.001f) return false;
    out.x = (g_scr.w * 0.5f) * (1.0f + cx / cw);
    out.y = (g_scr.h * 0.5f) * (1.0f - cy / cw);
    return out.x > -g_scr.w && out.x < g_scr.w * 2.f &&
           out.y > -g_scr.h && out.y < g_scr.h * 2.f;
}

// 世界相对矢量 -> 雷达平面
inline Vec2 RadarMap(const Vec3& local, float yawDeg, const Vec3& target,
                     float range, float radius) {
    Vec3 rel = target - local;
    float a = -yawDeg * 3.14159265f / 180.0f;
    float rx =  rel.x * cosf(a) - rel.z * sinf(a);
    float rz =  rel.x * sinf(a) + rel.z * cosf(a);
    float s  = radius / (range < 1.f ? 1.f : range);
    return { rx * s, -rz * s };
}

inline ImU32 lerpColor(ImU32 a, ImU32 b, float t) {
    t = std::clamp(t, 0.f, 1.f);
    auto ch = [](ImU32 c, int s) { return (int)((c >> s) & 0xFF); };
    int r = (int)(ch(a,0) * (1-t) + ch(b,0) * t);
    int g = (int)(ch(a,8) * (1-t) + ch(b,8) * t);
    int bl= (int)(ch(a,16) * (1-t) + ch(b,16) * t);
    int al= (int)(ch(a,24) * (1-t) + ch(b,24) * t);
    return IM_COL32(r, g, bl, al);
}
