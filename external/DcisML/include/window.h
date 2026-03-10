#pragma once

#include <cstdint>

namespace dcis {

struct Impl;

// ============================
// Events
// ============================

enum class AppEventType : uint8_t {
    None = 0,
    Quit,

    KeyDown,
    KeyUp,

    MouseMove,
    MouseDown,
    MouseUp,
    MouseWheel,

    Resize
};

enum class AppMouseButton : uint8_t {
    Left = 0,
    Right = 1,
    Middle = 2
};

struct AppEvent {
    AppEventType type = AppEventType::None;

    union {
        struct { int key; bool repeat; } key;
        struct { int x, y; } mouseMove;
        struct { AppMouseButton button; int x, y; } mouseBtn;
        struct { int delta; } wheel;    
        struct { int width, height; } resize;
    };
};

// ============================
// Time
// ============================

struct AppTimePoint {
    uint64_t ticks = 0;
};

AppTimePoint GetAppTimeNow();
double GetAppTimeSecondsBetween(AppTimePoint a, AppTimePoint b);
void SleepAppMs(uint32_t ms);

// ============================
// Window
// ============================

class AppWindow {
public:
    AppWindow() = default;
    ~AppWindow();

    AppWindow(const AppWindow&) = delete;
    AppWindow& operator=(const AppWindow&) = delete;

    AppWindow(AppWindow&& other) noexcept;
    AppWindow& operator=(AppWindow&& other) noexcept;

    static AppWindow CreateAppWindow(const char* title, int width, int height);

    bool IsValid() const;

    bool ProcessAppWindowEvents();

    const char** GetRequiredVulkanInstanceExtensions(uint32_t* exceptions_count, bool debug) const;
    bool CreateVulkanSurface(void* vkInstance, void* allocator, void** outSurface) const;

    void* GetNativeHwnd() const;
    void* GetNativeHinstance() const;

    bool PollAppEvent(AppEvent& outEvent);

    bool IsAppKeyDown(int key) const;
    bool IsAppMouseDown(AppMouseButton button) const;
    void GetAppMousePosition(int& outX, int& outY) const;
    void GetAppMouseDelta(int& outX, int& outY) const;

    void GetAppWindowSize(int& outW, int& outH) const;

    void PresentAppWindow(const uint32_t* pixels, int width, int height);

    void DestroyAppWindow();

private:
    Impl* impl_ = nullptr;
};

}
