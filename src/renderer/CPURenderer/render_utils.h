#pragma once

#include "render/camera.h"
#include "render/viewport.h"
#include "core/lerp.h"
#include "core/vertex.h"
#include "core/color.h"

#include <vector>
#include <algorithm>

static void ModelToClipVertex(const std::vector<Vertex3D>& models, const Mat4& MVP, const Mat4& MV, std::vector<VertexClip>& out)
{
    for (auto model : models) {
        VertexClip clip;
        clip.clip = MVP * Vec4(model.pos.x ,model.pos.y, model.pos.z, 1.0f);
        clip.color = UnpackColor(model.color);
        clip.uv = model.uv;
        Vec4 normal4 = (MV * Vec4(model.normal, 0));
        clip.normal = normal4.XYZ().Normalized();
        out.push_back(clip);
    }
}

static void ClipByPlane(std::vector<VertexClip>& poly, float (*Fn)(const VertexClip&)){
    std::vector<VertexClip> out;
    int size = poly.size();
    for (int i = 0; i < size; i++) {
        const VertexClip& S = poly[(i-1 + size) % size];
        const VertexClip& E = poly[i];

        float fS = Fn(S);
        float fE = Fn(E);
        bool inS = fS >= 0.0f;
        bool inE = fE >= 0.0f;

        if (inS && inE) { out.push_back(E); continue;}
        if (!inS && !inE) continue;

        float denom = fS - fE;
        if (std::fabs(denom) < 1e-8f) continue;

        float t = fS / denom;

        VertexClip I = LerpVertexClip(S, E, t);

        if (!inS && inE) { out.push_back(I); out.push_back(E); continue;}
        if (inS && !inE) { out.push_back(I); continue;}
    }
    poly = std::move(out);
}


static bool ClipToScreenVertex(const VertexClip& v, const Viewport& vp, VertexScreen& out)
{
    if (v.clip.w <= 0.0f) return false;

    const float invW = 1.0f / v.clip.w;

    const float ndcX = v.clip.x * invW;
    const float ndcY = v.clip.y * invW;
    const float ndcZ = v.clip.z * invW;

    out.x = (int)((ndcX + 1.0f) * 0.5f * vp.width);
    out.y = (int)((1.0f - (ndcY + 1.0f) * 0.5f) * vp.height);

    out.z = ndcZ;
    out.invW = invW;
    out.colOverW = v.color * invW;
    out.uvOverW = v.uv * invW;
    out.nOverW = v.normal * invW;
    return true;
}