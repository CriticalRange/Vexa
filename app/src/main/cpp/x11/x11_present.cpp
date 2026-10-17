// Wire layouts: X.Org xorgproto 2024.1 dri3proto.h, presentproto.h and Xproto.h.
// AHB modifier 1255 handshake: Termux:X11 fa3a8b430e2896a19f44c99a9cb056254615ae06,
// lorie/src/main/cpp/lorie/InitOutput.c:loriePixmapFromFds. This is a pinned
// Termux-specific transport, not a standard DRM modifier or general Linux WSI.
#include "x11_present.h"

#include <android/hardware_buffer.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <deque>
#include <fcntl.h>
#include <limits>
#include <mutex>
#include <poll.h>
#include <sstream>
#include <sys/socket.h>
#include <sys/un.h>
#include <unordered_map>
#include <unistd.h>

namespace Vexa::X11 {
namespace {
using Bytes = std::vector<uint8_t>;
using Clock = std::chrono::steady_clock;
using Deadline = Clock::time_point;
constexpr auto IoLimit = std::chrono::seconds(2);
constexpr size_t MaxPacket = 65536;
constexpr size_t MaxEvents = 256;
uint16_t Get16(const uint8_t *p) { return uint16_t(p[0]) | (uint16_t(p[1]) << 8); }
uint32_t Get32(const uint8_t *p) { return Get16(p) | (uint32_t(Get16(p + 2)) << 16); }
void Put16(Bytes &b, size_t at, uint16_t v) { b[at] = v; b[at + 1] = v >> 8; }
void Put32(Bytes &b, size_t at, uint32_t v) { Put16(b, at, v); Put16(b, at + 2, v >> 16); }
void Put64(Bytes &b, size_t at, uint64_t v) { Put32(b, at, uint32_t(v)); Put32(b, at + 4, uint32_t(v >> 32)); }
Bytes Request(uint8_t opcode, uint8_t minor, size_t bytes) {
    Bytes b(bytes); b[0] = opcode; b[1] = minor; Put16(b, 2, uint16_t(bytes / 4)); return b;
}
int Milliseconds(Deadline deadline) {
    const auto left = deadline - Clock::now();
    if (left <= Clock::duration::zero()) return 0;
    return int(std::min<int64_t>(2000, std::chrono::duration_cast<std::chrono::milliseconds>(left).count() + 1));
}
struct Fd {
    int value = -1;
    ~Fd() { if (value >= 0) close(value); }
};
}

struct PresentConnection::Impl {
    std::mutex mutex;
    int fd = -1;
    uint32_t base = 0, mask = 0, root = 0, visual = 0;
    uint8_t rootDepth = 0, dri3 = 0, present = 0;
    uint16_t maxRequest = 0;
    uint64_t xid = 1, sequence = 0;
    Bytes received;
    std::deque<Bytes> events;
    struct Window { uint32_t event; uint8_t depth; };
    struct Pixmap { uint32_t window, serial = 0; bool busy = false; };
    std::unordered_map<uint32_t, Window> windows;
    std::unordered_map<uint32_t, Pixmap> pixmaps;
    std::deque<std::pair<uint16_t, std::string>> requests;
    ~Impl() { if (fd >= 0) close(fd); }

    bool Fail(std::string &error, const std::string &message, bool disconnect = true) {
        error = message;
        if (disconnect && fd >= 0) { close(fd); fd = -1; }
        return false;
    }
    bool Ready(std::string &error) { return fd >= 0 || Fail(error, "X11 connection is closed", false); }
    void Reset() {
        if (fd >= 0) close(fd);
        fd = -1; base = mask = root = visual = 0; rootDepth = dri3 = present = 0;
        maxRequest = 0; xid = 1; sequence = 0;
        received.clear(); events.clear(); windows.clear(); pixmaps.clear(); requests.clear();
    }
    bool Poll(int descriptor, short requested, Deadline deadline, std::string &error, bool fatal = true) {
        for (;;) {
            pollfd p{descriptor, requested, 0};
            const int result = poll(&p, 1, Milliseconds(deadline));
            if (result < 0 && errno == EINTR) { if (Clock::now() >= deadline) return Fail(error, "X11 I/O timed out", fatal); continue; }
            if (result < 0) return Fail(error, "X11 poll: " + std::string(strerror(errno)), fatal);
            if (result == 0) return Fail(error, "X11 I/O timed out", fatal);
            if (p.revents & requested) return true;
            return Fail(error, "X11 socket disconnected", fatal);
        }
    }
    bool Write(const Bytes &b, Deadline deadline, std::string &error, int sendFd = -1) {
        size_t offset = 0;
        while (offset < b.size()) {
            if (!Poll(fd, POLLOUT, deadline, error)) return false;
            iovec iov{const_cast<uint8_t *>(b.data() + offset), b.size() - offset};
            msghdr msg{}; msg.msg_iov = &iov; msg.msg_iovlen = 1;
            alignas(cmsghdr) std::array<uint8_t, CMSG_SPACE(sizeof(int))> control{};
            if (sendFd >= 0) {
                msg.msg_control = control.data(); msg.msg_controllen = control.size();
                auto *c = CMSG_FIRSTHDR(&msg);
                c->cmsg_level = SOL_SOCKET; c->cmsg_type = SCM_RIGHTS; c->cmsg_len = CMSG_LEN(sizeof(int));
                std::memcpy(CMSG_DATA(c), &sendFd, sizeof(sendFd));
            }
            const auto n = sendmsg(fd, &msg, MSG_NOSIGNAL | MSG_DONTWAIT);
            if (n < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) {
                if (Clock::now() >= deadline) return Fail(error, "X11 send timed out");
                continue;
            }
            if (n <= 0) return Fail(error, "X11 send: " + std::string(n == 0 ? "closed" : strerror(errno)));
            offset += size_t(n);
            sendFd = -1; // SCM_RIGHTS must accompany only the first successful write.
        }
        return true;
    }
    bool ReadMore(Deadline deadline, std::string &error, bool timeoutFatal) {
        if (!Poll(fd, POLLIN, deadline, error, timeoutFatal)) return false;
        std::array<uint8_t, 8192> bytes;
        alignas(cmsghdr) std::array<uint8_t, CMSG_SPACE(16 * sizeof(int))> control{};
        iovec iov{bytes.data(), bytes.size()};
        msghdr msg{}; msg.msg_iov = &iov; msg.msg_iovlen = 1;
        msg.msg_control = control.data(); msg.msg_controllen = control.size();
        ssize_t n;
        do { n = recvmsg(fd, &msg, MSG_DONTWAIT | MSG_CMSG_CLOEXEC); } while (n < 0 && errno == EINTR && Clock::now() < deadline);
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return ReadMore(deadline, error, timeoutFatal);
        if (n <= 0) return Fail(error, "X11 receive: " + std::string(n == 0 ? "closed" : strerror(errno)));
        bool hasFds = false;
        for (auto *c = CMSG_FIRSTHDR(&msg); c; c = CMSG_NXTHDR(&msg, c)) {
            if (c->cmsg_level != SOL_SOCKET || c->cmsg_type != SCM_RIGHTS || c->cmsg_len < CMSG_LEN(0)) continue;
            const size_t count = (c->cmsg_len - CMSG_LEN(0)) / sizeof(int);
            for (size_t i = 0; i < count; ++i) { int receivedFd; std::memcpy(&receivedFd, CMSG_DATA(c) + i * sizeof(int), sizeof(int)); close(receivedFd); }
            hasFds = true;
        }
        if (hasFds || (msg.msg_flags & (MSG_CTRUNC | MSG_TRUNC))) return Fail(error, "Unexpected/truncated X11 ancillary data");
        if (received.size() + size_t(n) > MaxPacket + bytes.size()) return Fail(error, "X11 receive buffer exceeds limit");
        received.insert(received.end(), bytes.begin(), bytes.begin() + n);
        return true;
    }
    bool ReadBytes(size_t count, Bytes &out, Deadline deadline, std::string &error, bool timeoutFatal = true) {
        while (received.size() < count) if (!ReadMore(deadline, error, timeoutFatal)) return false;
        out.assign(received.begin(), received.begin() + count);
        received.erase(received.begin(), received.begin() + count);
        return true;
    }
    bool Packet(Bytes &out, Deadline deadline, std::string &error, bool timeoutFatal = true) {
        while (received.size() < 32) if (!ReadMore(deadline, error, timeoutFatal)) return false;
        size_t count = 32;
        const uint8_t type = received[0] & 0x7f;
        if (type == 1 || type == 35) {
            const uint32_t extra = Get32(received.data() + 4);
            if (extra > (MaxPacket - 32) / 4) return Fail(error, "X11 packet length exceeds limit");
            count += size_t(extra) * 4;
        }
        return ReadBytes(count, out, deadline, error, timeoutFatal);
    }
    bool ErrorPacket(const Bytes &b, std::string &error) {
        std::ostringstream text;
        const auto seq = Get16(b.data() + 2);
        text << "X11 protocol error code=" << unsigned(b[1]) << " sequence=" << seq
             << " major=" << unsigned(b[10]) << " minor=" << Get16(b.data() + 8)
             << " resource=" << Get32(b.data() + 4);
        for (auto it = requests.rbegin(); it != requests.rend(); ++it)
            if (it->first == seq) { text << " request=" << it->second; break; }
        return Fail(error, text.str());
    }
    bool Dispatch(Bytes packet, std::string &error) {
        if (packet[0] == 0) return ErrorPacket(packet, error);
        if ((packet[0] & 0x7f) == 1) return Fail(error, "Unexpected X11 reply");
        if (packet[0] == 35 && packet[1] == present) {
            const auto type = Get16(packet.data() + 8);
            if ((type == 1 && packet.size() != 40) || (type == 2 && packet.size() != 32))
                return Fail(error, "Malformed Present notification");
            if (type == 1 || type == 2) {
                const auto w = windows.find(Get32(packet.data() + 16));
                if (w != windows.end() && w->second.event == Get32(packet.data() + 12)) {
                    if (type == 1) return true; // CompleteNotify is consumed, not mistaken for buffer idle.
                    auto p = pixmaps.find(Get32(packet.data() + 24));
                    if (p != pixmaps.end() && p->second.window == w->first &&
                        p->second.serial == Get32(packet.data() + 20)) {
                        p->second.busy = false;
                        return true;
                    }
                }
            }
        }
        if (events.size() >= MaxEvents) return Fail(error, "Unconsumed X11 event queue exceeds limit");
        events.push_back(std::move(packet));
        return true;
    }
    bool Send(const Bytes &b, const char *name, Deadline deadline, uint16_t &seq, std::string &error, int sendFd = -1) {
        if (!Ready(error)) return false;
        if (b.size() % 4 || b.size() / 4 > maxRequest) return Fail(error, "X11 request exceeds server limit");
        seq = uint16_t(++sequence);
        requests.emplace_back(seq, name); if (requests.size() > 256) requests.pop_front();
        return Write(b, deadline, error, sendFd);
    }
    bool Reply(uint16_t seq, Bytes &reply, Deadline deadline, std::string &error) {
        for (;;) {
            Bytes b;
            if (!Packet(b, deadline, error)) return false;
            if (b[0] == 1 && Get16(b.data() + 2) == seq) { reply = std::move(b); return true; }
            if (!Dispatch(std::move(b), error)) return false;
        }
    }
    bool RoundTrip(Deadline deadline, std::string &error) {
        uint16_t seq; Bytes reply;
        return Send(Request(43, 0, 4), "GetInputFocus barrier", deadline, seq, error) && Reply(seq, reply, deadline, error) &&
               (reply.size() == 32 || Fail(error, "Malformed GetInputFocus reply"));
    }
    uint32_t NewId(std::string &error) {
        uint64_t ordinal = xid++, remaining = ordinal;
        uint32_t expanded = 0;
        for (unsigned bit = 0; bit < 32; ++bit) if (mask & (uint32_t(1) << bit)) {
            expanded |= uint32_t(remaining & 1) << bit; remaining >>= 1;
        }
        if (remaining || !ordinal) { Fail(error, "X11 resource ID space exhausted"); return 0; }
        return base | expanded;
    }
    bool Extension(const char *name, uint8_t &opcode, Deadline deadline, std::string &error) {
        const size_t size = strlen(name);
        auto b = Request(98, 0, 8 + ((size + 3) & ~size_t(3)));
        Put16(b, 4, uint16_t(size)); std::memcpy(b.data() + 8, name, size);
        uint16_t seq; Bytes reply;
        if (!Send(b, name, deadline, seq, error) || !Reply(seq, reply, deadline, error)) return false;
        if (reply.size() != 32 || !reply[8] || reply[9] < 128) return Fail(error, std::string("X11 extension unavailable: ") + name);
        opcode = reply[9]; return true;
    }
    bool Version(uint8_t opcode, uint32_t major, uint32_t minor, Deadline deadline, std::string &error) {
        auto b = Request(opcode, 0, 12); Put32(b, 4, major); Put32(b, 8, minor);
        uint16_t seq; Bytes reply;
        if (!Send(b, "QueryVersion", deadline, seq, error) || !Reply(seq, reply, deadline, error)) return false;
        if (reply.size() != 32 || Get32(reply.data() + 8) < major ||
            (Get32(reply.data() + 8) == major && Get32(reply.data() + 12) < minor))
            return Fail(error, "X11 extension version too old");
        return true;
    }
    bool EnsureWindow(uint32_t window, Deadline deadline, std::string &error) {
        if (windows.count(window)) return true;
        auto b = Request(14, 0, 8); Put32(b, 4, window);
        uint16_t seq; Bytes reply;
        if (!Send(b, "GetGeometry", deadline, seq, error) || !Reply(seq, reply, deadline, error)) return false;
        if (reply.size() != 32 || (reply[1] != 24 && reply[1] != 32)) return Fail(error, "X11 target depth must be 24 or 32");
        const auto eid = NewId(error); if (!eid) return false;
        b = Request(present, 3, 16); Put32(b, 4, eid); Put32(b, 8, window); Put32(b, 12, 6); // Complete + Idle
        if (!Send(b, "PresentSelectInput", deadline, seq, error) || !RoundTrip(deadline, error)) return false;
        windows.emplace(window, Window{eid, reply[1]});
        return true;
    }
};

PresentConnection::PresentConnection() : impl_(std::make_unique<Impl>()) {}
PresentConnection::~PresentConnection() = default;

bool PresentConnection::Connect(const std::string &socket, std::string &error) {
    auto &i = *impl_; std::lock_guard<std::mutex> lock(i.mutex); i.Reset(); error.clear();
    std::string path = socket;
    if (path.empty()) { if (const char *p = getenv("VEXA_X11_SOCKET")) path = p; }
    if (path.empty()) { if (const char *p = getenv("VEXA_ROOTFS")) path = std::string(p) + "/tmp/.X11-unix/X0"; }
    sockaddr_un address{}; address.sun_family = AF_UNIX;
    if (path.empty() || path.size() >= sizeof(address.sun_path) || path.find('\0') != std::string::npos)
        return i.Fail(error, "Invalid X11 Unix socket pathname");
    std::memcpy(address.sun_path, path.c_str(), path.size() + 1);
    i.fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (i.fd < 0) return i.Fail(error, "X11 socket: " + std::string(strerror(errno)));
    const auto deadline = Clock::now() + IoLimit;
    if (::connect(i.fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0) {
        if (errno != EINPROGRESS && errno != EAGAIN) return i.Fail(error, "X11 connect: " + std::string(strerror(errno)));
        if (!i.Poll(i.fd, POLLOUT, deadline, error)) return false;
        int result = 0; socklen_t size = sizeof(result);
        if (getsockopt(i.fd, SOL_SOCKET, SO_ERROR, &result, &size) != 0 || result)
            return i.Fail(error, "X11 connect: " + std::string(strerror(result ? result : errno)));
    }
    Bytes hello(12); hello[0] = 'l'; Put16(hello, 2, 11);
    if (!i.Write(hello, deadline, error)) return false;
    Bytes prefix, setup;
    if (!i.ReadBytes(8, prefix, deadline, error)) return false;
    const size_t setupSize = size_t(Get16(prefix.data() + 6)) * 4;
    if (setupSize > MaxPacket || !i.ReadBytes(setupSize, setup, deadline, error))
        return i.Fail(error, error.empty() ? "X11 setup exceeds limit" : error);
    if (prefix[0] != 1) {
        const size_t reasonSize = std::min<size_t>(prefix[1], setup.size());
        return i.Fail(error, "X11 setup refused/authentication required: " + std::string(setup.begin(), setup.begin() + reasonSize));
    }
    if (Get16(prefix.data() + 2) != 11 || setup.size() < 32 || !setup[20]) return i.Fail(error, "Malformed X11 setup");
    i.base = Get32(setup.data() + 4); i.mask = Get32(setup.data() + 8);
    i.maxRequest = Get16(setup.data() + 18);
    if (!i.mask || (i.base & i.mask) || i.maxRequest < 18) return i.Fail(error, "Invalid X11 resource/request limits");
    const size_t formats = 32 + ((size_t(Get16(setup.data() + 16)) + 3) & ~size_t(3));
    size_t offset = formats + size_t(setup[21]) * 8;
    if (offset > setup.size()) return i.Fail(error, "Truncated X11 vendor/pixmap formats");
    for (unsigned screen = 0; screen < setup[20]; ++screen) {
        if (setup.size() - offset < 40) return i.Fail(error, "Truncated X11 screen");
        const auto *s = setup.data() + offset;
        if (screen == 0) { i.root = Get32(s); i.visual = Get32(s + 32); i.rootDepth = s[38]; }
        const auto depths = s[39]; offset += 40;
        for (unsigned depth = 0; depth < depths; ++depth) {
            if (setup.size() - offset < 8) return i.Fail(error, "Truncated X11 depth");
            const size_t visuals = Get16(setup.data() + offset + 2);
            offset += 8;
            if (visuals > (setup.size() - offset) / 24) return i.Fail(error, "Truncated X11 visual list");
            offset += visuals * 24;
        }
    }
    bool format32 = false;
    for (unsigned n = 0; n < setup[21]; ++n)
        if (setup[formats + n * 8] == i.rootDepth && setup[formats + n * 8 + 1] == 32) format32 = true;
    if (!i.root || !i.visual || (i.rootDepth != 24 && i.rootDepth != 32) || !format32)
        return i.Fail(error, "X11 default screen needs a 24/32-depth, 32-bpp visual");
    return i.Extension("DRI3", i.dri3, deadline, error) && i.Version(i.dri3, 1, 2, deadline, error) &&
           i.Extension("Present", i.present, deadline, error) && i.Version(i.present, 1, 0, deadline, error);
}

uint32_t PresentConnection::CreateWindow(uint32_t width, uint32_t height, std::string &error) {
    auto &i = *impl_; std::lock_guard<std::mutex> lock(i.mutex); error.clear();
    if (!i.Ready(error)) return 0;
    if (!width || !height || width > 32767 || height > 32767) { i.Fail(error, "Invalid X11 window dimensions", false); return 0; }
    const auto window = i.NewId(error); if (!window) return 0;
    auto b = Request(1, i.rootDepth, 32); Put32(b, 4, window); Put32(b, 8, i.root);
    Put16(b, 16, uint16_t(width)); Put16(b, 18, uint16_t(height)); Put16(b, 22, 1); // InputOutput
    Put32(b, 24, i.visual);
    const auto deadline = Clock::now() + IoLimit; uint16_t seq;
    if (!i.Send(b, "CreateWindow", deadline, seq, error)) return 0;
    b = Request(8, 0, 8); Put32(b, 4, window);
    if (!i.Send(b, "MapWindow", deadline, seq, error) || !i.RoundTrip(deadline, error) || !i.EnsureWindow(window, deadline, error)) return 0;
    return window;
}

bool PresentConnection::ImportHardwareBuffer(uint32_t window, uint32_t width, uint32_t height,
                                             AHardwareBuffer *buffer, uint32_t &pixmap, std::string &error) {
    auto &i = *impl_; std::lock_guard<std::mutex> lock(i.mutex); error.clear(); pixmap = 0;
    if (!i.Ready(error)) return false;
    if (!window || !buffer || !width || !height || width > 32767 || height > 32767)
        return i.Fail(error, "Invalid X11 AHardwareBuffer import arguments", false);
    AHardwareBuffer_Desc desc{}; AHardwareBuffer_describe(buffer, &desc);
    // BGRA=5 is accepted by the pinned server but is not in the public NDK enum.
    if (desc.width != width || desc.height != height || desc.layers != 1 || desc.stride < width ||
        desc.stride > std::numeric_limits<uint32_t>::max() / 4 ||
        (desc.format != AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM && desc.format != AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM && desc.format != 5))
        return i.Fail(error, "AHardwareBuffer dimensions/stride/layers/format unsupported by pinned X11 server", false);
    const auto deadline = Clock::now() + IoLimit;
    if (!i.EnsureWindow(window, deadline, error)) return false;
    const auto id = i.NewId(error); if (!id) return false;
    int pair[2];
    // Keep the SERVER endpoint blocking: the server acknowledges then blocks
    // receiving its handle. Only our endpoint is nonblocking and deadline driven.
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, pair) != 0)
        return i.Fail(error, "AHB socketpair: " + std::string(strerror(errno)), false);
    Fd local{pair[0]}, remote{pair[1]};
    const auto flags = fcntl(local.value, F_GETFL);
    if (flags < 0 || fcntl(local.value, F_SETFL, flags | O_NONBLOCK) != 0)
        return i.Fail(error, "AHB socket flags: " + std::string(strerror(errno)), false);
    auto b = Request(i.dri3, 7, 64); Put32(b, 4, id); Put32(b, 8, window); b[12] = 1;
    Put16(b, 16, uint16_t(width)); Put16(b, 18, uint16_t(height)); Put32(b, 20, desc.stride * 4);
    b[52] = i.windows.at(window).depth; b[53] = 32; Put64(b, 56, 1255);
    uint16_t seq;
    if (!i.Send(b, "DRI3PixmapFromBuffers AHB1255", deadline, seq, error, remote.value)) return false;
    close(remote.value); remote.value = -1;
    bool acknowledged = false;
    while (!acknowledged) {
        pollfd p[2]{{local.value, POLLIN, 0}, {i.fd, POLLIN, 0}};
        const int result = poll(p, 2, Milliseconds(deadline));
        if (result < 0 && errno == EINTR && Clock::now() < deadline) continue;
        if (result <= 0) return i.Fail(error, result == 0 ? "AHB import acknowledgement timed out" : "AHB import poll failed");
        if (p[1].revents) {
            Bytes packet;
            if (!i.Packet(packet, deadline, error) || !i.Dispatch(std::move(packet), error)) return false;
        }
        if (p[0].revents & POLLIN) {
            uint8_t ack = 0;
            const auto n = recv(local.value, &ack, 1, MSG_DONTWAIT);
            if (n < 0 && (errno == EAGAIN || errno == EINTR)) continue;
            if (n != 1 || ack != 1) return i.Fail(error, "Invalid AHB import acknowledgement");
            acknowledged = true;
        } else if (p[0].revents & (POLLERR | POLLHUP | POLLNVAL)) return i.Fail(error, "AHB import socket closed before acknowledgement");
        if (!acknowledged && Clock::now() >= deadline) return i.Fail(error, "AHB import acknowledgement timed out");
    }
    if (!i.Poll(local.value, POLLOUT, deadline, error)) return false;
    // This opaque Android API must never block indefinitely: our socket remains
    // O_NONBLOCK. If its one-shot send fails, abort rather than duplicate data.
    const int result = AHardwareBuffer_sendHandleToUnixSocket(buffer, local.value);
    if (result != 0) return i.Fail(error, "AHardwareBuffer_sendHandleToUnixSocket failed: " + std::to_string(result));
    if (!i.RoundTrip(deadline, error)) return false;
    i.pixmaps.emplace(id, Impl::Pixmap{window, 0, false}); pixmap = id;
    return true;
}

bool PresentConnection::Present(uint32_t window, uint32_t pixmap, uint32_t serial, std::string &error) {
    auto &i = *impl_; std::lock_guard<std::mutex> lock(i.mutex); error.clear();
    if (!i.Ready(error)) return false;
    auto p = i.pixmaps.find(pixmap);
    if (p == i.pixmaps.end() || p->second.window != window) return i.Fail(error, "Pixmap does not belong to this presentation window", false);
    if (p->second.busy) return i.Fail(error, "Pixmap is still in use by X11; wait for IdleNotify before reuse", false);
    const auto deadline = Clock::now() + IoLimit;
    auto b = Request(i.present, 1, 72); Put32(b, 4, window); Put32(b, 8, pixmap); Put32(b, 12, serial);
    p->second.serial = serial; p->second.busy = true;
    uint16_t seq;
    return i.Send(b, "PresentPixmap", deadline, seq, error) && i.RoundTrip(deadline, error);
}

bool PresentConnection::WaitIdle(uint32_t pixmap, uint64_t timeoutNs, std::string &error) {
    auto &i = *impl_; std::lock_guard<std::mutex> lock(i.mutex); error.clear();
    if (!i.Ready(error)) return false;
    auto p = i.pixmaps.find(pixmap);
    if (p == i.pixmaps.end()) return i.Fail(error, "Unknown X11 pixmap", false);
    const auto bounded = std::min<uint64_t>(timeoutNs, std::chrono::duration_cast<std::chrono::nanoseconds>(IoLimit).count());
    const auto deadline = Clock::now() + std::chrono::nanoseconds(bounded);
    while (p->second.busy) {
        Bytes packet;
        if (!i.Packet(packet, deadline, error, false) || !i.Dispatch(std::move(packet), error)) return false;
        if (p->second.busy && Clock::now() >= deadline) return i.Fail(error, "X11 pixmap idle wait timed out", false);
    }
    return true;
}

bool PresentConnection::FreePixmap(uint32_t pixmap, std::string &error) {
    auto &i = *impl_; std::lock_guard<std::mutex> lock(i.mutex); error.clear();
    if (!i.Ready(error)) return false;
    auto p = i.pixmaps.find(pixmap);
    if (p == i.pixmaps.end()) return i.Fail(error, "Unknown X11 pixmap", false);
    if (p->second.busy) return i.Fail(error, "Cannot free a busy X11 pixmap", false);
    const auto deadline = Clock::now() + IoLimit;
    auto b = Request(54, 0, 8); Put32(b, 4, pixmap); uint16_t seq;
    if (!i.Send(b, "FreePixmap", deadline, seq, error) || !i.RoundTrip(deadline, error)) return false;
    i.pixmaps.erase(p); return true;
}

bool PresentConnection::TakeEvent(std::vector<uint8_t> &event) {
    auto &i = *impl_; std::lock_guard<std::mutex> lock(i.mutex);
    if (i.events.empty()) return false;
    event = std::move(i.events.front()); i.events.pop_front(); return true;
}
}
