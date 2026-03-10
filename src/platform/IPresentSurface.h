#pragma once

#include <cstdint>

namespace dcis { struct AppWindow; }

class IPresentSurface {
public:
    virtual ~IPresentSurface() = default;
    virtual void Present(const void* pixels, uint32_t w, uint32_t h) = 0;
    virtual void GetSize(uint32_t& w, uint32_t& h) const = 0;
};

class INativeWindowSurface : public IPresentSurface {
public:
    virtual void* GetNativeHwnd() const = 0;
    virtual void* GetNativeHinstance() const = 0;
    virtual void* GetExtensions(uint32_t* extensions_count, bool debug) const = 0;
};