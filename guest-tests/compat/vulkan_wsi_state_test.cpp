// Android-native regression harness: exercise the production WSI state machine
// with deterministic GPU/X events. No driver or server behavior is inferred.
#include <atomic>
#include <cassert>
#include <future>
#include <poll.h>
#include <unordered_set>
#include "../../app/src/main/cpp/x11/vulkan_wsi.cpp"

using namespace std::chrono_literals;
static std::atomic<int> driverActive{}, driverMax{}, sent{}, checkedPresents{}, deviceIdles{};
static std::atomic<bool> gpuBlocked{}, autoIdle{true}, queueWaiting{}, releaseQueue{};
static std::atomic<bool> failNextSubmit{};
static std::atomic<unsigned> failAfter{}, semaphoreSignals{};
static VkFence blockedFence{};
static std::mutex sentMutex;
static std::unordered_set<uint32_t> sentPixmaps;

extern "C" {
VKAPI_ATTR VkResult VKAPI_CALL vkResetCommandBuffer(VkCommandBuffer, VkCommandBufferResetFlags) { return VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL vkBeginCommandBuffer(VkCommandBuffer, const VkCommandBufferBeginInfo*) { return VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL vkEndCommandBuffer(VkCommandBuffer) { return VK_SUCCESS; }
VKAPI_ATTR void VKAPI_CALL vkCmdPipelineBarrier(VkCommandBuffer, VkPipelineStageFlags, VkPipelineStageFlags,
    VkDependencyFlags, uint32_t, const VkMemoryBarrier*, uint32_t, const VkBufferMemoryBarrier*, uint32_t, const VkImageMemoryBarrier*) {}
VKAPI_ATTR VkResult VKAPI_CALL vkResetFences(VkDevice, uint32_t, const VkFence*) { return VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL vkWaitForFences(VkDevice, uint32_t count, const VkFence* fences, VkBool32, uint64_t timeout) {
  if (count && fences[0] == blockedFence && gpuBlocked) {
    std::this_thread::sleep_for(std::chrono::nanoseconds(std::min<uint64_t>(timeout, 1000000)));
    return VK_TIMEOUT;
  }
  return VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit(VkQueue, uint32_t count, const VkSubmitInfo* info, VkFence) {
  if (failNextSubmit.exchange(false) || (failAfter && --failAfter == 0)) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
  for (uint32_t n = 0; n < count; ++n) semaphoreSignals += info[n].signalSemaphoreCount;
  const int active = ++driverActive;
  int old = driverMax;
  while (old < active && !driverMax.compare_exchange_weak(old, active)) {}
  std::this_thread::sleep_for(2ms);
  --driverActive;
  return VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL vkQueueWaitIdle(VkQueue) {
  queueWaiting = true;
  while (!releaseQueue) std::this_thread::sleep_for(1ms);
  return VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL vkDeviceWaitIdle(VkDevice) { ++deviceIdles; return VK_SUCCESS; }
VKAPI_ATTR void VKAPI_CALL vkDestroyFence(VkDevice, VkFence, const VkAllocationCallbacks*) {}
VKAPI_ATTR void VKAPI_CALL vkDestroyCommandPool(VkDevice, VkCommandPool, const VkAllocationCallbacks*) {}
VKAPI_ATTR void VKAPI_CALL vkDestroyImage(VkDevice, VkImage, const VkAllocationCallbacks*) {}
}

namespace Vexa::X11 {
struct PresentConnection::Impl {};
PresentConnection::PresentConnection() : impl_(std::make_unique<Impl>()) {}
PresentConnection::~PresentConnection() = default;
bool PresentConnection::Connect(const std::string&, std::string&) { return true; }
bool PresentConnection::ImportHardwareBuffer(uint32_t, uint32_t, uint32_t, AHardwareBuffer*, uint32_t&, std::string&) { return false; }
bool PresentConnection::Present(uint32_t, uint32_t, uint32_t, std::string&) { ++checkedPresents; return true; }
bool PresentConnection::QueuePresent(uint32_t, uint32_t pixmap, uint32_t, std::string&) {
  std::lock_guard lock(sentMutex); sentPixmaps.insert(pixmap); ++sent; return true;
}
bool PresentConnection::PollEvents(uint64_t timeout, int wakeFd, std::string&) {
  pollfd fd{wakeFd, POLLIN, 0}; poll(&fd, 1, timeout ? 5 : 0); return true;
}
bool PresentConnection::IsIdle(uint32_t pixmap) {
  std::lock_guard lock(sentMutex); return autoIdle && sentPixmaps.count(pixmap);
}
bool PresentConnection::WaitIdle(uint32_t, uint64_t, std::string&) { return true; }
bool PresentConnection::FreePixmap(uint32_t, std::string&) { return true; }
void PresentConnection::Disconnect() {}
}

static uintptr_t nextHandle = 100;
static auto MakeChain(bool worker = false) {
  auto s = std::make_shared<Swapchain>();
  s->device = Handle<VkDevice>((void*)1); s->queue = Handle<VkQueue>((void*)2); s->family = 0; s->window = 1;
  s->wakeFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  for (unsigned n = 0; n < 2; ++n) {
    s->images.emplace_back(); auto &i = s->images.back();
    i.pixmap = ++nextHandle;
    i.presentFence = std::make_shared<PresentFence>(); i.presentFence->device = s->device;
    i.presentFence->fence = Handle<VkFence>((void*)++nextHandle);
    i.acquireFence = Handle<VkFence>((void*)++nextHandle);
  }
  const auto handle = Handle<VkSwapchainKHR>(s.get());
  { std::lock_guard lock(mutex); chains[handle] = s; }
  if (worker) s->worker = std::thread(PresentWorker, std::ref(*s));
  return std::pair{handle, s};
}
static void PresentOne(VkSwapchainKHR handle, Swapchain &s) {
  uint32_t index = 0; VkResult perImage = VK_ERROR_UNKNOWN;
  s.images[0].acquired = true;
  VkPresentInfoKHR info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
  info.swapchainCount = 1; info.pSwapchains = &handle; info.pImageIndices = &index; info.pResults = &perImage;
  assert(Present(s.queue, &info) == VK_SUCCESS && perImage == VK_SUCCESS);
}
template<class Predicate> static void Eventually(Predicate predicate) {
  auto end = std::chrono::steady_clock::now() + 2s;
  while (!predicate() && std::chrono::steady_clock::now() < end) std::this_thread::sleep_for(1ms);
  assert(predicate());
}
int main() {
  library = dlopen(nullptr, RTLD_NOW); // --export-dynamic supplies the mock ICD.
  assert(FN(vkQueueSubmit) == &vkQueueSubmit);
  auto [handle, s] = MakeChain();
  for (auto &i : s->images) i.pending = true;
  uint32_t index = 99;
  assert(Acquire(s->device, handle, 0, {}, {}, &index) == VK_NOT_READY);
  auto start = std::chrono::steady_clock::now();
  assert(Acquire(s->device, handle, 40000000, {}, {}, &index) == VK_TIMEOUT);
  assert(std::chrono::steady_clock::now() - start >= 35ms);
  auto acquire = std::async(std::launch::async, [&] { return Acquire(s->device, handle, UINT64_MAX, {}, {}, &index); });
  assert(acquire.wait_for(30ms) == std::future_status::timeout);
  // A waiting Acquire must not hold the global registry mutex.
  { std::lock_guard lock(mutex); for (unsigned n = 0; n < 256; ++n) chains[Handle<VkSwapchainKHR>((void*)(uintptr_t(10000 + n)))] = std::make_shared<Swapchain>(); }
  { std::lock_guard lock(s->stateMutex); s->images[0].pending = false; s->changed.notify_all(); }
  assert(acquire.get() == VK_SUCCESS && index == 0);
  { std::lock_guard lock(s->stateMutex); LatchError(*s, VK_ERROR_SURFACE_LOST_KHR); }
  assert(Acquire(s->device, handle, 0, {}, {}, &index) == VK_ERROR_SURFACE_LOST_KHR);
  assert(Acquire(s->device, handle, 0, {}, {}, &index) == VK_ERROR_SURFACE_LOST_KHR);
  VkPresentInfoKHR failed{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
  failed.swapchainCount = 1; failed.pSwapchains = &handle; failed.pImageIndices = &index;
  assert(Present(s->queue, &failed) == VK_ERROR_SURFACE_LOST_KHR);
  assert(Present(s->queue, &failed) == VK_ERROR_SURFACE_LOST_KHR);
  DestroySwapchain(s->device, handle, nullptr);
  puts("PASS Acquire poll, finite deadline, infinite wait, registry progress, sticky error");

  driverMax = 0;
  auto a = std::async(std::launch::async, [&] { for (int n = 0; n < 20; ++n) Submit(s->queue, 0, nullptr, {}); });
  auto b = std::async(std::launch::async, [&] { for (int n = 0; n < 20; ++n) Submit(s->queue, 0, nullptr, {}); });
  a.get(); b.get(); assert(driverMax == 1);
  auto waiter = std::async(std::launch::async, [&] { return QueueIdle(s->queue); });
  Eventually([] { return queueWaiting.load(); });
  auto unrelated = std::async(std::launch::async, [&] { return Submit(Handle<VkQueue>((void*)3), 0, nullptr, {}); });
  const bool progressed = unrelated.wait_for(100ms) == std::future_status::ready;
  releaseQueue = true; waiter.get(); unrelated.get(); assert(progressed);
  puts("PASS same-queue exclusion, unrelated queue progress during QueueWaitIdle");
  auto [hc, sc] = MakeChain(); sc->images[1].pending = true;
  driverMax = 0;
  auto submitter = std::async(std::launch::async, [&] { for (int n = 0; n < 20; ++n) Submit(sc->queue, 0, nullptr, {}); });
  for (int n = 0; n < 20; ++n) {
    uint32_t image;
    assert(Acquire(sc->device, hc, UINT64_MAX, {}, {}, &image) == VK_SUCCESS);
    std::lock_guard lock(sc->stateMutex); sc->images[image].acquired = false;
  }
  submitter.get(); assert(driverMax == 1);
  failNextSubmit = true;
  assert(Acquire(sc->device, hc, 0, {}, {}, &index) == VK_ERROR_OUT_OF_DEVICE_MEMORY);
  assert(!sc->images[0].acquireSubmitted);
  DestroySwapchain(sc->device, hc, nullptr);
  puts("PASS internal Acquire submit shares the guest queue gate");
  puts("PASS failed submit after fence reset does not leave a phantom cleanup wait");
  auto [hd, sd] = MakeChain(); sd->images[1].pending = true;
  const auto signalsBefore = semaphoreSignals.load(); failAfter = 2;
  assert(Acquire(sd->device, hd, 0, Handle<VkSemaphore>((void*)91), Handle<VkFence>((void*)92), &index) == VK_ERROR_OUT_OF_DEVICE_MEMORY);
  assert(sd->images[0].acquireSubmitted && semaphoreSignals == signalsBefore);
  DestroySwapchain(sd->device, hd, nullptr);
  puts("PASS failed caller-fence marker leaves caller semaphore untouched");

  auto [ha, sa] = MakeChain(true); auto [hb, sb] = MakeChain(true);
  blockedFence = sa->images[0].presentFence->fence; gpuBlocked = true; autoIdle = false;
  PresentOne(ha, *sa); PresentOne(hb, *sb);
  Eventually([&] { std::lock_guard lock(sb->stateMutex); return sb->images[0].busy; });
  { std::lock_guard lock(sa->stateMutex); assert(sa->images[0].pending); }
  assert(checkedPresents == 0); // WSI never uses the round-trip API.
  auto destroy = std::async(std::launch::async, [&] { DestroySwapchain(sa->device, ha, nullptr); });
  assert(destroy.wait_for(30ms) == std::future_status::timeout);
  { std::lock_guard lock(mutex);
    assert(!chains.count(ha));
    for (unsigned n = 0; n < 512; ++n) chains[Handle<VkSwapchainKHR>((void*)(uintptr_t(20000 + n)))] = std::make_shared<Swapchain>();
  }
  gpuBlocked = false; Wake(*sa); destroy.get();
  autoIdle = true; Wake(*sb);
  Eventually([&] { std::lock_guard lock(sb->stateMutex); return !sb->images[0].busy; });
  DestroySwapchain(sb->device, hb, nullptr);
  assert(deviceIdles == 0);
  puts("PASS independent workers, GPU completion before send, IdleNotify reuse, no DeviceWaitIdle cleanup");
  puts("PASS DestroySwapchain drains submitted work while concurrent creation rehashes registry");
}
