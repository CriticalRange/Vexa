#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct AHardwareBuffer;

namespace Vexa::X11 {
// A dedicated little-endian X11 connection. Each call is serialized and bounded;
// use from the presentation worker, never the Android UI thread. No Xlib needed.
class PresentConnection {
public:
    PresentConnection();
    ~PresentConnection();
    PresentConnection(const PresentConnection &) = delete;
    PresentConnection &operator=(const PresentConnection &) = delete;

    // An empty path selects VEXA_X11_SOCKET, then VEXA_ROOTFS/tmp/.X11-unix/X0.
    bool Connect(const std::string &socket, std::string &error);
    uint32_t CreateWindow(uint32_t width, uint32_t height, std::string &error);
    bool ImportHardwareBuffer(uint32_t window, uint32_t width, uint32_t height,
                              AHardwareBuffer *buffer, uint32_t &pixmap, std::string &error);
    bool Present(uint32_t window, uint32_t pixmap, uint32_t serial, std::string &error);
    // timeoutNs=0 polls available packets; UINT64_MAX is clamped to two seconds.
    // False on timeout leaves the pixmap busy and must not permit its reuse.
    bool WaitIdle(uint32_t pixmap, uint64_t timeoutNs, std::string &error);
    bool FreePixmap(uint32_t pixmap, std::string &error);
    // Unrelated events remain available, rather than being discarded by waits.
    bool TakeEvent(std::vector<uint8_t> &event);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
