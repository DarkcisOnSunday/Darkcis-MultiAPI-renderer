#define _USE_MATH_DEFINES

#include "DcisML/include/window.h"

#include "core/scene.h"
#include "render/camera.h"
#include "app/demo_scene.h"
#include "renderer/IRenderer.h"
#include "renderer/CPURenderer/CPURendererBackend.h"
#include "renderer/VulkanRenderer/VulkanRendererBackend.h"

#include <cmath>
#include <memory>


int main()
{
    const int width = 800;
    const int height = 800;

    auto window = dcis::AppWindow::CreateAppWindow("SoftRender Demo", width, height);
    if (!window.IsValid())
        return 1;

    WindowSurfaceAdapter surface(window);

    std::unique_ptr<IRenderer> renderer = std::make_unique<VulkanRendererBackend>(surface);
    renderer->OnResize(width, height);

    Scene scene;
    Camera cam;
    cam.SetPerspective(70.0f / M_PI * 180.0f, (float)width / (float)height, 0.05f, 40.0f);

    dcis::AppTimePoint start = dcis::GetAppTimeNow();
    double prevT = 0;

    while (window.ProcessAppWindowEvents()) {
        const float moveSpeed = 1.5f;
        const float mouseSens = 0.0025f; 

        double t = dcis::GetAppTimeSecondsBetween(start, dcis::GetAppTimeNow());
        float dt = float(t - prevT); prevT = t;
        if (dt > 0.1f) dt = 0.1f;

        Vec3 move(0,0,0);
        if (window.IsAppKeyDown('A')) move.x -= 1.0f;
        if (window.IsAppKeyDown('D')) move.x += 1.0f;
        if (window.IsAppKeyDown('W')) move.z += 1.0f;
        if (window.IsAppKeyDown('S')) move.z -= 1.0f;
        if (window.IsAppKeyDown(0x20)) move.y += 1.0f;
        if (window.IsAppKeyDown(0x11)) move.y -= 1.0f;


        if (window.IsAppMouseDown(dcis::AppMouseButton::Right)) {
            int dx, dy;
            window.GetAppMouseDelta(dx, dy);
            cam.AddYawPitch((float)dx * mouseSens, (float)dy * mouseSens);
        }

        if (move.LengthSquared() > 0.0f) {
            move = move.Normalized() * (moveSpeed * dt);
            cam.MoveLocal(move);
        }

        cam.UpdateView();
        UpdateScene(scene, t);

        uint32_t w, h; surface.GetSize(w, h);
        FrameContext fc{ dt, t, w, h };

        renderer->BeginFrame(fc);
        renderer->RenderScene(scene, cam);
        renderer->EndFrame();
    }

    window.DestroyAppWindow();
    return 0;
}