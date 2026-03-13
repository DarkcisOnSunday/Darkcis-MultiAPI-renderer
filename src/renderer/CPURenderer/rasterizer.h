#pragma once

#include <cstdint>
#include "core/vertex.h"

class FrameBuffer;
struct Material;
struct Vec3;

class Rasterizer {
private:
    void DrawHorizontalLine(FrameBuffer& image, int x0, int x1, int y, uint32_t color);
    void DrawHorizontalLine(FrameBuffer& image, int x0, int x1, float z0, float z1, int y, uint32_t color);
    void DrawVerticalLine(FrameBuffer& image, int y0, int y1, int x, uint32_t color);
    void DrawVerticalLine(FrameBuffer& image, int y0, int y1, float z0, float z1, int x, uint32_t color);

public:
    void DrawLine2D(FrameBuffer& image, int x0, int y0, int x1, int y1, uint32_t color);
    void DrawLine3D(FrameBuffer& image, VertexScreen v1, VertexScreen v2);
    void DrawTriangle2D(FrameBuffer& image, int x1, int y1, int x2, int y2, int x3, int y3, uint32_t color);
    void DrawTriangle3D(FrameBuffer& image, VertexScreen& v1, VertexScreen& v2, VertexScreen& v3, const Material& texture, const Vec3& Light, float ambient);
    void DrawTriangle3DScanline(FrameBuffer& image, VertexScreen v1, VertexScreen v2, VertexScreen v3);
    void DrawRect(FrameBuffer& image, int x, int y, int width, int height, uint32_t color); 
};
