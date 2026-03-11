#define _USE_MATH_DEFINES

#include "demo_scene.h"
#include "render/camera.h"
#include "core/color.h"
#include "core/mesh.h"
#include "core/material.h"
#include "core/scene.h"

#include <cmath>


/*auto UV = [](float x, float y) {
    return Vec2((x + 0.25f) / 0.5f, (y + 0.25f) / 0.5f);
};

static Mesh BuildCubeMesh()
{
    Mesh m;

    m.vertices = {
        {{ 0.25f,  0.25f,  0.25f}, RED,     UV( 0.25f,  0.25f)},
        {{-0.25f,  0.25f,  0.25f}, GREEN,   UV(-0.25f,  0.25f)},
        {{-0.25f, -0.25f,  0.25f}, BLUE,    UV(-0.25f, -0.25f)},
        {{ 0.25f, -0.25f,  0.25f}, WHITE,   UV( 0.25f, -0.25f)},

        {{ 0.25f,  0.25f, -0.25f}, YELLOW,  UV( 0.25f,  0.25f)},
        {{-0.25f,  0.25f, -0.25f}, CYAN,    UV(-0.25f,  0.25f)},
        {{-0.25f, -0.25f, -0.25f}, MAGENTA, UV(-0.25f, -0.25f)},
        {{ 0.25f, -0.25f, -0.25f}, ORANGE,  UV( 0.25f, -0.25f)}
    };

    m.indices = {
        // front (z=+)
        0,1,2,  0,2,3,
        // back (z=-)
        4,6,5,  4,7,6,
        // left (x=-)
        1,5,6,  1,6,2,
        // right (x=+)
        0,3,7,  0,7,4,
        // top (y=+)
        0,4,5,  0,5,1,
        // bottom (y=-)
        3,2,6,  3,6,7
    };

    return m;
}*/

static Mesh BuildCubeMesh_UV()
{
    Mesh m;

    const float s = 0.25f;

    m.vertices = {
        // ----- Front  (z = +s)
        {{-s, -s, +s}, BLACK,  {0,0}, {0, 0, 1}},
        {{+s, -s, +s}, BLACK,  {1,0}, {0, 0, 1}},
        {{+s, +s, +s}, BLACK,  {1,1}, {0, 0, 1}},
        {{-s, +s, +s}, BLACK,  {0,1}, {0, 0, 1}},

        // ----- Back   (z = -s)
        {{+s, -s, -s}, BLACK,  {0,0}, {0, 0, -1}},
        {{-s, -s, -s}, BLACK,  {1,0}, {0, 0, -1}},
        {{-s, +s, -s}, BLACK,  {1,1}, {0, 0, -1}},
        {{+s, +s, -s}, BLACK,  {0,1}, {0, 0, -1}},

        // ----- Left   (x = -s)
        {{-s, -s, -s}, BLACK,  {0,0}, {-1, 0, 0}},
        {{-s, -s, +s}, BLACK,  {1,0}, {-1, 0, 0}},
        {{-s, +s, +s}, BLACK,  {1,1}, {-1, 0, 0}},
        {{-s, +s, -s}, BLACK,  {0,1}, {-1, 0, 0}},

        // ----- Right  (x = +s)
        {{+s, -s, +s}, BLACK,  {0,0}, {1, 0, 0}},
        {{+s, -s, -s}, BLACK,  {1,0}, {1, 0, 0}},
        {{+s, +s, -s}, BLACK,  {1,1}, {1, 0, 0}},
        {{+s, +s, +s}, BLACK,  {0,1}, {1, 0, 0}},

        // ----- Top    (y = +s)
        {{-s, +s, +s}, BLACK,  {0,0}, {0, 1, 0}},
        {{+s, +s, +s}, BLACK,  {1,0}, {0, 1, 0}},
        {{+s, +s, -s}, BLACK,  {1,1}, {0, 1, 0}},
        {{-s, +s, -s}, BLACK,  {0,1}, {0, 1, 0}},

        // ----- Bottom (y = -s)
        {{-s, -s, -s}, BLACK,  {0,0}, {0, -1, 0}},
        {{+s, -s, -s}, BLACK,  {1,0}, {0, -1, 0}},
        {{+s, -s, +s}, BLACK,  {1,1}, {0, -1, 0}},
        {{-s, -s, +s}, BLACK,  {0,1}, {0, -1, 0}},
    };

    m.indices = {
        0,1,2,  0,2,3,       // front
        4,5,6,  4,6,7,       // back
        8,9,10, 8,10,11,     // left
        12,13,14, 12,14,15,  // right
        16,17,18, 16,18,19,  // top
        20,21,22, 20,22,23   // bottom
    };

    return m;
}

static Texture2D tex = Texture2D::MakeChecker(256,256,32);

void UpdateScene(Scene& scene, double time) {
    scene.Clear();

    static Mesh cube = BuildCubeMesh_UV();
    static Material matChecher;
    matChecher.albedo = &tex;
    matChecher.mode = TextureMode::Replace;

    //float dz = 1.0f;
    //float angle = (float)(M_PI * time);

    RenderObject obj1;
    obj1.mesh = &cube;
    obj1.material = &matChecher;
    obj1.model = Mat4::Translation(0,0,1.0f);
    scene.objects.push_back(obj1);

    RenderObject obj2;
    obj2.mesh = &cube;
    obj2.material = &matChecher;
    obj2.model =
        Mat4::Translation(1.3f, 0.4f, 2.0f) *
        Mat4::RotationY((float)time * 0.7f);
    scene.objects.push_back(obj2);
}