// Production wire parser against a socketpair, including fragmented X events.
#include <memory>
#include <string>
#include <vector>
#define private public
#include "../../app/src/main/cpp/x11/x11_present.h"
#undef private
#include "../../app/src/main/cpp/x11/x11_present.cpp"
#include <cassert>

int main() {
  using namespace Vexa::X11;
  int sockets[2]; assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
  PresentConnection connection;
  auto &i = *connection.impl_;
  i.fd = sockets[0]; i.present = 150; i.maxRequest = 65535;
  i.windows.emplace(42, PresentConnection::Impl::Window{55, 24});
  i.pixmaps.emplace(7, PresentConnection::Impl::Pixmap{42, 0, false});
  std::string error;
  assert(connection.QueuePresent(42, 7, 1, error));
  uint8_t request[128]; const auto size = recv(sockets[1], request, sizeof(request), MSG_DONTWAIT);
  assert(size == 72 && request[0] == 150 && request[1] == 1);
  assert(recv(sockets[1], request, sizeof(request), MSG_DONTWAIT) == -1 && errno == EAGAIN);
  assert(!connection.IsIdle(7));
  Bytes idle(32); idle[0] = 35; idle[1] = 150;
  Put16(idle, 8, 2); Put32(idle, 12, 55); Put32(idle, 16, 42);
  Put32(idle, 20, 1); Put32(idle, 24, 7);
  assert(write(sockets[1], idle.data(), 13) == 13);
  assert(connection.PollEvents(0, -1, error) && !connection.IsIdle(7));
  assert(write(sockets[1], idle.data() + 13, 19) == 19);
  assert(connection.PollEvents(0, -1, error) && connection.IsIdle(7));
  puts("PASS asynchronous PresentPixmap without GetInputFocus, fragmented IdleNotify");
  Bytes failure(32); failure[0] = 0; failure[1] = 3;
  assert(write(sockets[1], failure.data(), failure.size()) == 32);
  assert(!connection.PollEvents(0, -1, error));
  assert(error.find("X11 protocol error") != std::string::npos && i.fd == -1);
  close(sockets[1]);
  puts("PASS asynchronous protocol error closes the connection");
}
