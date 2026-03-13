#pragma once

#include "core/vertex.h"

#include <cstdint>
#include <vector>

struct Mesh {
    std::vector<Vertex3D> vertices;
    std::vector<uint32_t> indices;
};