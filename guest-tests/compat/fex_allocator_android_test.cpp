// Exercise the Android FEX allocator independently from the app process.
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>
#include <sys/mman.h>
#include <unistd.h>

namespace FEXCore::Allocator {
void* malloc(size_t);
void* calloc(size_t, size_t);
void* realloc(void*, size_t);
void free(void*);
void* memalign(size_t, size_t);
void* valloc(size_t);
void InitializeThread();
void InitializeAllocator(size_t);
void SetupAllocatorHooks(void* (*)(void*, size_t, int, int, int, off_t), int (*)(void*, size_t));
}
static std::atomic<int> wrongBackendCalls {};
// ARM64 Android requires at least 64-byte TLS alignment in native executables.
alignas(64) thread_local volatile unsigned char testTlsAlignment;
static void* WrongMap(void*, size_t, int, int, int, off_t) { ++wrongBackendCalls; return MAP_FAILED; }
static int WrongUnmap(void*, size_t) { ++wrongBackendCalls; return -1; }

int main() {
  testTlsAlignment = 1;
  using namespace FEXCore::Allocator;
  // Allocate before guest setup, as config and other FEX containers may do.
  auto* old = static_cast<unsigned char*>(FEXCore::Allocator::malloc(2 * 1024 * 1024));
  assert(old);
  std::memset(old, 0x5a, 2 * 1024 * 1024);
  void* systemAllocation = ::malloc(12345);
  assert(systemAllocation);
  const long page = sysconf(_SC_PAGESIZE);
  assert(page > 0);
  SetupAllocatorHooks(WrongMap, WrongUnmap);
  InitializeAllocator(static_cast<size_t>(page));
  assert(old[0] == 0x5a && old[2 * 1024 * 1024 - 1] == 0x5a);
  FEXCore::Allocator::free(old);
  ::free(systemAllocation);

  auto* zero = static_cast<unsigned char*>(FEXCore::Allocator::calloc(256, 32));
  assert(zero && zero[0] == 0 && zero[8191] == 0);
  zero[0] = 37;
  zero = static_cast<unsigned char*>(FEXCore::Allocator::realloc(zero, 32768));
  assert(zero && zero[0] == 37);
  FEXCore::Allocator::free(zero);
  void* aligned = FEXCore::Allocator::memalign(4096, 8192);
  assert(aligned && reinterpret_cast<uintptr_t>(aligned) % 4096 == 0);
  FEXCore::Allocator::free(aligned);
  void* pageAligned = FEXCore::Allocator::valloc(9000);
  assert(pageAligned && reinterpret_cast<uintptr_t>(pageAligned) % page == 0);
  FEXCore::Allocator::free(pageAligned);

  std::vector<void*> crossThread(8);
  std::vector<std::thread> threads;
  for (size_t i = 0; i < crossThread.size(); ++i) {
    threads.emplace_back([&, i] {
      InitializeThread();
      for (int n = 0; n < 5000; ++n) {
        auto* p = static_cast<unsigned char*>(FEXCore::Allocator::malloc(64 + n % 4096));
        assert(p); p[0] = 42; FEXCore::Allocator::free(p);
      }
      crossThread[i] = FEXCore::Allocator::malloc(128 * 1024);
      assert(crossThread[i]);
    });
  }
  for (auto& thread : threads) thread.join();
  for (void* p : crossThread) FEXCore::Allocator::free(p);
  assert(wrongBackendCalls == 0);
  std::printf("PASS FEX Android rpmalloc: early allocation, stable mapping, bionic coexistence, alignment, threads, cross-thread free; page=%ld\n", page);
}
