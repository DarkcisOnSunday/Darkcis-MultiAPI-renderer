#pragma once

#include "math/vec2.h"
#include "math/vec3.h"
#include "math/vec4.h"

#include <cstdint>

struct VertexScreen {
    int x, y;
    float z;
    float invW;
    Vec4 colOverW;
    Vec2 uvOverW;
    Vec3 nOverW;
};

struct VertexClip {
    Vec4 clip;
    Vec4 color;
    Vec2 uv;
    Vec3 normal;  
};

struct Vertex3D {
    Vec3 pos;
    uint32_t color;
    Vec2 uv;
    Vec3 normal;
};