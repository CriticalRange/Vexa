// Production pre-rotation submit path with a fake ICD. Android NDK, -lvulkan.
#define VK_USE_64_BIT_PTR_DEFINES 0
#include <vulkan/vulkan.h>
#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <unistd.h>
#define LDR_PTR(n) n
static VkPhysicalDevice g_vexa_physical_device {};
static bool VexaRotProfileEnabled() { return false; }
struct VexaVkWallTime {
  static uint64_t ThreadCpuNs() {
    timespec t {}; clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
    return uint64_t(t.tv_sec) * 1000000000 + t.tv_nsec;
  }
};
#include "../../../FEX/ThunkLibs/libvulkan/vulkan_prerotate.inc"

static VkResult waitResult, resetResult, submitResult;
static unsigned waits, resets, submits;
static bool expectTimeline, expectFence;
static VkQueue queue = reinterpret_cast<VkQueue>(0x1000);
static VkSemaphore guestWait = 10;
static VkResult Wait(VkDevice, uint32_t, const VkFence*, VkBool32, uint64_t) { ++waits; return waitResult; }
static VkResult Reset(VkDevice, uint32_t, const VkFence*) { ++resets; return resetResult; }
static VkResult Submit(VkQueue q, uint32_t n, const VkSubmitInfo* info, VkFence fence) {
  ++submits; assert(q == queue && n == 1);
  assert(info->waitSemaphoreCount == 1 && info->pWaitSemaphores[0] == guestWait);
  assert(info->pWaitDstStageMask[0] == VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
  assert((fence != VK_NULL_HANDLE) == expectFence);
  assert(info->pSignalSemaphores[0] == 20); // WSI always waits on binary Done.
  if (expectTimeline) {
    auto* timeline = static_cast<const VkTimelineSemaphoreSubmitInfo*>(info->pNext);
    assert(timeline && timeline->waitSemaphoreValueCount == 1 && timeline->pWaitSemaphoreValues[0] == 0);
    assert(timeline->signalSemaphoreValueCount == 2 && info->pSignalSemaphores[1] == 30);
    assert(timeline->pSignalSemaphoreValues[0] == 0 && timeline->pSignalSemaphoreValues[1] == 8);
  } else assert(info->pNext == nullptr && info->signalSemaphoreCount == 1);
  return submitResult;
}
static VexaRotSwapchain* Setup(bool timeline, bool async) {
  VexaRotSwapchains.clear(); waits = resets = submits = 0;
  waitResult = resetResult = submitResult = VK_SUCCESS;
  expectTimeline = timeline; expectFence = !async;
  auto rot = std::make_unique<VexaRotSwapchain>();
  rot->Profile = false; rot->Recorded = true; rot->RecordedQueue = queue;
  rot->Commands.resize(1); rot->Fences = {40}; rot->FencePending = {true}; rot->Done = {20};
  rot->Timeline = timeline ? 30 : 0; rot->Async = async;
  rot->ModeCheck = 1; // Dynamic test flag is polled at the next sample boundary.
  rot->TimelineValue = 7; rot->ImageValues = {7};
  rot->Fns.vkWaitForFences = Wait; rot->Fns.vkResetFences = Reset; rot->Fns.vkQueueSubmit = Submit;
  auto* result = rot.get(); VexaRotSwapchains[1] = std::move(rot); return result;
}
int main() {
  VkSwapchainKHR chain = 1; uint32_t image = 0;
  VkPresentInfoKHR info {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
  info.swapchainCount = 1; info.pSwapchains = &chain; info.pImageIndices = &image;
  info.waitSemaphoreCount = 1; info.pWaitSemaphores = &guestWait;
  std::vector<VkSemaphore> done;
  auto* rot = Setup(false, false);
  assert(VexaRotSubmit(queue, info, done) == VK_SUCCESS && waits == 1 && resets == 1 && submits == 1);
  assert(rot->FencePending[0] && done.size() == 1 && done[0] == 20);
  puts("PASS fence path preserves guest waits and binary WSI signal");
  rot = Setup(false, false); waitResult = VK_ERROR_DEVICE_LOST; done.clear();
  assert(VexaRotSubmit(queue, info, done) == waitResult && rot->Lost && resets == 0 && submits == 0 && done.empty());
  puts("PASS failed wait never resets or submits");
  rot = Setup(false, false); resetResult = VK_ERROR_OUT_OF_HOST_MEMORY;
  assert(VexaRotSubmit(queue, info, done) == resetResult && !rot->Lost && !rot->FencePending[0] && submits == 0);
  rot = Setup(false, false); submitResult = VK_ERROR_OUT_OF_HOST_MEMORY;
  assert(VexaRotSubmit(queue, info, done) == submitResult && !rot->Lost && !rot->FencePending[0]);
  puts("PASS reset/submit OOM has no phantom fence wait or false device loss");
  rot = Setup(true, true); done.clear();
  assert(VexaRotSubmit(queue, info, done) == VK_SUCCESS && waits == 0 && resets == 0 && submits == 1);
  assert(rot->TimelineValue == 8 && rot->ImageValues[0] == 8 && done.size() == 1 && done[0] == 20);
  puts("PASS timeline skips CPU wait/reset and keeps binary present semaphore");
  rot = Setup(true, false); done.clear();
  assert(VexaRotSubmit(queue, info, done) == VK_SUCCESS && waits == 1 && resets == 1 && submits == 1 && rot->TimelineValue == 8);
  puts("PASS controlled fence mode still signals retirement timeline");
  rot = Setup(true, true); done.clear(); submitResult = VK_ERROR_OUT_OF_HOST_MEMORY;
  assert(VexaRotSubmit(queue, info, done) == submitResult && rot->TimelineValue == 7 && rot->ImageValues[0] == 7 && done.empty());
  rot = Setup(true, true); done.clear();
  assert(VexaRotSubmit(reinterpret_cast<VkQueue>(0x2000), info, done) == VK_ERROR_OUT_OF_DATE_KHR && submits == 0);
  puts("PASS failed timeline submit never advances value; another queue rejected");
  VexaRotSwapchains.clear();
}
