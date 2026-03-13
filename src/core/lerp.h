#pragma once

#include "math/vec2.h"
#include "math/vec3.h"
#include "math/vec4.h"
#include "core/vertex.h"
#include "core/color.h"

#include <cstdint>
#include <algorithm>

float LerpFloat(float a, float b, float t) {
    return a + (b - a) * t;
}

static Vec4 LerpVec4(const Vec4& a, const Vec4& b, float t) {
    return a + (b - a) * t;
}

static Vec3 LerpVec3(const Vec3& a, const Vec3& b, float t) {
    return a + (b - a) * t;
}

static Vec2 LerpVec2(const Vec2& a, const Vec2& b, float t) {
    return a + (b - a) * t;
}

static VertexClip LerpVertexClip(const VertexClip& a, const VertexClip& b, float t)
{
    VertexClip out;
    out.clip  = LerpVec4(a.clip, b.clip, t);
    out.color = LerpVec4(a.color, b.color, t);
    out.uv = LerpVec2(a.uv, b.uv, t);
    out.normal = LerpVec3(a.normal, a.normal, t);
    return out;
}