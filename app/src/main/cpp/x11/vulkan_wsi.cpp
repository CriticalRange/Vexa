#define VK_USE_PLATFORM_ANDROID_KHR
#include "vulkan_wsi.h"
#include "x11_present.h"
#include <algorithm>
#include <android/hardware_buffer.h>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <dlfcn.h>
#include <iterator>
#include <memory>
#include <mutex>
#include <sys/eventfd.h>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>
#include <unistd.h>

namespace {
// Synthetic handles are private to this WSI. Never pass them to the Android
// ICD.
struct Surface {
  uint32_t window;
};
struct PresentFence {
  VkDevice device;
  VkFence fence{};
  ~PresentFence();
};
struct Image {
  VkImage image{};
  VkDeviceMemory memory{};
  AHardwareBuffer *buffer{};
  uint32_t pixmap{};
  bool acquired = false, busy = false, initialized = false;
  // In flight between caller-side submission and X11 accepting the pixmap.
  bool pending = false;
  std::shared_ptr<PresentFence> presentFence;
  VkFence acquireFence{};
  bool acquireSubmitted = false;
  VkCommandBuffer acquireCommand{}, releaseCommand{};
};
struct Swapchain {
  VkDevice device;
  VkQueue queue;
  uint32_t family;
  VkCommandPool pool{};
  uint32_t window, serial = 0;
  // Terminal failures stay latched until this swapchain is destroyed.
  VkResult deferredError = VK_SUCCESS;
  bool destroyed = false;
  std::mutex stateMutex;
  std::condition_variable changed;
  struct Job {
    std::shared_ptr<PresentFence> completion;
    uint32_t image, serial;
  };
  std::deque<Job> jobs;
  std::thread worker;
  int wakeFd = -1;
  ~Swapchain() { if (wakeFd >= 0) close(wakeFd); }
  Vexa::X11::PresentConnection connection;
  std::vector<Image> images;
};
std::mutex mutex;
std::unordered_map<VkDevice, uint32_t> deviceFamilies;
std::unordered_map<VkSurfaceKHR, std::unique_ptr<Surface>> surfaces;
std::unordered_map<VkSwapchainKHR, std::shared_ptr<Swapchain>> chains;
// Guest queue entry points and WSI's internal ownership submits share these
// gates. Acquire is allowed to run concurrently with guest queue operations.
struct QueueState {
  VkDevice device{};
  std::mutex mutex;
};
std::mutex queuesMutex;
std::unordered_map<VkQueue, std::shared_ptr<QueueState>> queues;
void *library = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
template <typename T> T Load(const char *name) {
  return reinterpret_cast<T>(dlsym(library, name));
}
#define FN(n) Load<PFN_##n>(#n)
PresentFence::~PresentFence() {
  if (fence) FN(vkDestroyFence)(device, fence, nullptr);
}
std::shared_ptr<QueueState> Queue(VkQueue queue) {
  std::lock_guard lock(queuesMutex);
  auto &state = queues[queue];
  if (!state) state = std::make_shared<QueueState>();
  return state;
}
VkResult Submit(VkQueue queue, uint32_t count, const VkSubmitInfo *info, VkFence fence) {
  auto state = Queue(queue);
  std::lock_guard lock(state->mutex);
  return FN(vkQueueSubmit)(queue, count, info, fence);
}
VkResult Submit2Impl(VkQueue queue, uint32_t count, const VkSubmitInfo2 *info, VkFence fence, bool khr) {
  auto state = Queue(queue);
  std::lock_guard lock(state->mutex);
  auto fn = khr ? reinterpret_cast<PFN_vkQueueSubmit2>(FN(vkQueueSubmit2KHR)) : FN(vkQueueSubmit2);
  return fn ? fn(queue, count, info, fence) : VK_ERROR_EXTENSION_NOT_PRESENT;
}
VkResult Submit2(VkQueue q, uint32_t n, const VkSubmitInfo2 *info, VkFence f) {
  return Submit2Impl(q, n, info, f, false);
}
VkResult Submit2KHR(VkQueue q, uint32_t n, const VkSubmitInfo2 *info, VkFence f) {
  return Submit2Impl(q, n, info, f, true);
}
VkResult BindSparse(VkQueue queue, uint32_t count, const VkBindSparseInfo *info, VkFence fence) {
  auto state = Queue(queue);
  std::lock_guard lock(state->mutex);
  return FN(vkQueueBindSparse)(queue, count, info, fence);
}
VkResult QueueIdle(VkQueue queue) {
  auto state = Queue(queue);
  std::lock_guard lock(state->mutex);
  return FN(vkQueueWaitIdle)(queue);
}
VkResult DeviceIdle(VkDevice device) {
  std::vector<std::shared_ptr<QueueState>> states;
  {
    std::lock_guard lock(queuesMutex);
    for (auto &[queue, state] : queues)
      if (state->device == device) states.push_back(state);
  }
  // Every DeviceIdle uses the same order. A queue wait only locks its own
  // queue, so an unrelated queue can still submit a semaphore signal.
  std::sort(states.begin(), states.end());
  std::vector<std::unique_lock<std::mutex>> locks;
  for (auto &state : states) locks.emplace_back(state->mutex);
  return FN(vkDeviceWaitIdle)(device);
}
template <class H, class P> H Handle(P *p) { return (H)(uintptr_t)p; }
VkResult Fail(const char *why, VkResult code = VK_ERROR_FEATURE_NOT_PRESENT) {
  fprintf(stderr, "[VEXA][X11-WSI] %s\n", why);
  return code;
}
void Wake(Swapchain &s) {
  uint64_t one = 1;
  (void)write(s.wakeFd, &one, sizeof(one));
}
void LatchError(Swapchain &s, VkResult error) {
  if (s.deferredError == VK_SUCCESS || error == VK_ERROR_DEVICE_LOST)
    s.deferredError = error;
  s.changed.notify_all();
}
void PresentWorker(Swapchain &s) {
  bool connectionFailed = false;
  for (;;) {
    uint64_t value;
    while (read(s.wakeFd, &value, sizeof(value)) == sizeof(value)) {}
    Swapchain::Job job;
    bool haveJob;
    {
      std::lock_guard lock(s.stateMutex);
      haveJob = !s.jobs.empty();
      if (s.destroyed && !haveJob) break;
      if (haveJob) job = s.jobs.front();
    }
    if (haveJob) {
      // Short bounded waits let this chain drain X events while GPU work is
      // pending. Independent swapchains have independent workers.
      VkResult r = FN(vkWaitForFences)(s.device, 1, &job.completion->fence,
                                     VK_TRUE, 1000000);
      if (r != VK_TIMEOUT) {
        std::string error;
        if (r == VK_SUCCESS && !connectionFailed &&
            !s.connection.QueuePresent(s.window, s.images[job.image].pixmap,
                                       job.serial, error)) {
          fprintf(stderr, "[VEXA][X11-WSI] present failed: %s\n",
                  error.c_str());
          connectionFailed = true;
        }
        if (r == VK_SUCCESS && connectionFailed) r = VK_ERROR_SURFACE_LOST_KHR;
        std::lock_guard lock(s.stateMutex);
        auto &i = s.images[job.image];
        i.pending = false;
        i.busy = r == VK_SUCCESS;
        if (r != VK_SUCCESS) LatchError(s, r);
        s.jobs.pop_front();
        s.changed.notify_all();
      }
    }
    std::string error;
    if (!connectionFailed && !s.connection.PollEvents(haveJob ? 0 : UINT64_MAX,
                                                     s.wakeFd, error)) {
      fprintf(stderr, "[VEXA][X11-WSI] event stream failed: %s\n", error.c_str());
      connectionFailed = true;
      std::lock_guard lock(s.stateMutex);
      LatchError(s, VK_ERROR_SURFACE_LOST_KHR);
    }
    if (connectionFailed) s.connection.Disconnect();
    {
      std::unique_lock lock(s.stateMutex);
      for (auto &i : s.images) {
        if (i.busy && !connectionFailed && s.connection.IsIdle(i.pixmap)) {
          i.busy = false;
          s.changed.notify_all();
        }
      }
      if (connectionFailed && s.jobs.empty())
        s.changed.wait(lock, [&] { return s.destroyed || !s.jobs.empty(); });
    }
  }
}
template <class T>
VkResult Enumerate(const std::vector<T> &v, uint32_t *count, T *out) {
  if (!count)
    return VK_ERROR_INITIALIZATION_FAILED;
  if (!out) {
    *count = v.size();
    return VK_SUCCESS;
  }
  uint32_t n = std::min(*count, uint32_t(v.size()));
  std::copy_n(v.begin(), n, out);
  *count = n;
  return n < v.size() ? VK_INCOMPLETE : VK_SUCCESS;
}
VkResult CreateSurface(VkInstance, uint32_t window, VkSurfaceKHR *out) {
  if (!window || !out)
    return VK_ERROR_INITIALIZATION_FAILED;
  std::lock_guard lock(mutex);
  auto p = std::make_unique<Surface>();
  p->window = window;
  *out = Handle<VkSurfaceKHR>(p.get());
  surfaces.emplace(*out, std::move(p));
  return VK_SUCCESS;
}
void DestroySurface(VkInstance, VkSurfaceKHR s, const VkAllocationCallbacks *) {
  std::lock_guard lock(mutex);
  surfaces.erase(s);
}
VkResult Support(VkPhysicalDevice p, uint32_t family, VkSurfaceKHR s,
                 VkBool32 *out) {
  std::lock_guard lock(mutex);
  if (!surfaces.count(s) || !out)
    return VK_ERROR_SURFACE_LOST_KHR;
  uint32_t n = 0;
  FN(vkGetPhysicalDeviceQueueFamilyProperties)(p, &n, nullptr);
  std::vector<VkQueueFamilyProperties> q(n);
  FN(vkGetPhysicalDeviceQueueFamilyProperties)(p, &n, q.data());
  *out = family < n && (q[family].queueFlags & VK_QUEUE_GRAPHICS_BIT);
  return VK_SUCCESS;
}
VkResult Caps(VkPhysicalDevice, VkSurfaceKHR s, VkSurfaceCapabilitiesKHR *out) {
  std::lock_guard lock(mutex);
  if (!out || !surfaces.count(s))
    return VK_ERROR_SURFACE_LOST_KHR;
  *out = {};
  out->minImageCount = 2;
  out->maxImageCount = 4;
  out->currentExtent = {UINT32_MAX, UINT32_MAX};
  out->minImageExtent = {1, 1};
  out->maxImageExtent = {8192, 8192};
  out->maxImageArrayLayers = 1;
  out->supportedTransforms = out->currentTransform =
      VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  out->supportedCompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  out->supportedUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                             VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                             VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  return VK_SUCCESS;
}
VkResult Formats(VkPhysicalDevice, VkSurfaceKHR surface, uint32_t *n,
                 VkSurfaceFormatKHR *out) {
  std::lock_guard lock(mutex);
  if (!surfaces.count(surface))
    return VK_ERROR_SURFACE_LOST_KHR;
  return Enumerate<VkSurfaceFormatKHR>(
      {{VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
       {VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}},
      n, out);
}
VkResult Modes(VkPhysicalDevice, VkSurfaceKHR surface, uint32_t *n,
               VkPresentModeKHR *out) {
  std::lock_guard lock(mutex);
  if (!surfaces.count(surface))
    return VK_ERROR_SURFACE_LOST_KHR;
  return Enumerate<VkPresentModeKHR>({VK_PRESENT_MODE_FIFO_KHR}, n, out);
}
VkResult Ownership(Swapchain &s, Image &i, bool acquire) {
  auto cmd = acquire ? i.acquireCommand : i.releaseCommand;
  auto result = FN(vkResetCommandBuffer)(cmd, 0);
  if (result != VK_SUCCESS)
    return result;
  VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  result = FN(vkBeginCommandBuffer)(cmd, &begin);
  if (result != VK_SUCCESS)
    return result;
  VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  barrier.image = i.image;
  barrier.oldLayout = acquire && !i.initialized ? VK_IMAGE_LAYOUT_UNDEFINED
                                                : VK_IMAGE_LAYOUT_GENERAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
  barrier.srcQueueFamilyIndex =
      acquire ? VK_QUEUE_FAMILY_FOREIGN_EXT : s.family;
  barrier.dstQueueFamilyIndex =
      acquire ? s.family : VK_QUEUE_FAMILY_FOREIGN_EXT;
  barrier.srcAccessMask = acquire ? 0 : VK_ACCESS_MEMORY_WRITE_BIT;
  barrier.dstAccessMask =
      acquire ? (VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT) : 0;
  barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  FN(vkCmdPipelineBarrier)(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                           VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0,
                           nullptr, 1, &barrier);
  return FN(vkEndCommandBuffer)(cmd);
}
void Cleanup(Swapchain &s) {
  // The worker has drained all release submissions. Only an acquired image
  // that was never presented can still have WSI-owned GPU work outstanding.
  for (auto &i : s.images)
    if (i.acquireSubmitted)
      FN(vkWaitForFences)(s.device, 1, &i.acquireFence, VK_TRUE, UINT64_MAX);
  std::string error;
  if (s.pool)
    FN(vkDestroyCommandPool)(s.device, s.pool, nullptr);
  for (auto &i : s.images) {
    i.presentFence.reset();
    if (i.acquireFence)
      FN(vkDestroyFence)(s.device, i.acquireFence, nullptr);
    if (i.busy)
      s.connection.WaitIdle(i.pixmap, UINT64_MAX, error);
    if (i.pixmap)
      s.connection.FreePixmap(i.pixmap, error);
    if (i.image)
      FN(vkDestroyImage)(s.device, i.image, nullptr);
    if (i.memory)
      FN(vkFreeMemory)(s.device, i.memory, nullptr);
    if (i.buffer)
      AHardwareBuffer_release(i.buffer);
  }
  s.connection.Disconnect();
}
VkResult CreateSwapchain(VkDevice device, const VkSwapchainCreateInfoKHR *info,
                         const VkAllocationCallbacks *, VkSwapchainKHR *out) {
  uint32_t window, family;
  {
    std::lock_guard lock(mutex);
    if (!info || !out || !surfaces.count(info->surface))
      return VK_ERROR_SURFACE_LOST_KHR;
    if (!deviceFamilies.count(device)) return Fail("No graphics queue registered");
    window = surfaces.at(info->surface)->window;
    family = deviceFamilies.at(device);
  }
  if (info->pNext || info->flags || info->imageArrayLayers != 1 ||
      !info->imageUsage ||
      (info->imageUsage &
       ~(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
         VK_IMAGE_USAGE_TRANSFER_SRC_BIT)) ||
      info->imageSharingMode != VK_SHARING_MODE_EXCLUSIVE ||
      info->presentMode != VK_PRESENT_MODE_FIFO_KHR ||
      info->preTransform != VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR ||
      info->compositeAlpha != VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR ||
      info->imageColorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR ||
      (info->imageFormat != VK_FORMAT_R8G8B8A8_UNORM &&
       info->imageFormat != VK_FORMAT_R8G8B8A8_SRGB) ||
      info->minImageCount < 2 || info->minImageCount > 4 ||
      !info->imageExtent.width || !info->imageExtent.height ||
      info->imageExtent.width > 8192 || info->imageExtent.height > 8192)
    return Fail("Unsupported swapchain parameters (FIFO RGBA exclusive only)");
  auto s = std::make_shared<Swapchain>();
  s->device = device;
  s->window = window;
  s->family = family;
  FN(vkGetDeviceQueue)(device, s->family, 0, &s->queue);
  VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  poolInfo.queueFamilyIndex = s->family;

  std::string error;
  if (!s->connection.Connect("", error))
    return Fail(error.c_str(), VK_ERROR_SURFACE_LOST_KHR);
  auto gdpa = FN(vkGetDeviceProcAddr);
  auto ahb = reinterpret_cast<PFN_vkGetAndroidHardwareBufferPropertiesANDROID>(
      gdpa(device, "vkGetAndroidHardwareBufferPropertiesANDROID"));
  if (!ahb)
    return Fail("AHB device extension not enabled",
                VK_ERROR_EXTENSION_NOT_PRESENT);
  VkResult result =
      FN(vkCreateCommandPool)(device, &poolInfo, nullptr, &s->pool);
  if (result != VK_SUCCESS)
    return result;
  VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
  for (uint32_t n = 0; n < info->minImageCount; ++n) {
    s->images.emplace_back();
    auto &i = s->images.back();
    i.presentFence = std::make_shared<PresentFence>();
    i.presentFence->device = device;
    result = FN(vkCreateFence)(device, &fenceInfo, nullptr, &i.presentFence->fence);
    if (result != VK_SUCCESS)
      break;
    result = FN(vkCreateFence)(device, &fenceInfo, nullptr, &i.acquireFence);
    if (result != VK_SUCCESS)
      break;
    VkCommandBufferAllocateInfo commands{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    commands.commandPool = s->pool;
    commands.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commands.commandBufferCount = 2;
    VkCommandBuffer buffers[2];
    result = FN(vkAllocateCommandBuffers)(device, &commands, buffers);
    if (result != VK_SUCCESS)
      break;
    i.acquireCommand = buffers[0];
    i.releaseCommand = buffers[1];
    AHardwareBuffer_Desc desc{};
    desc.width = info->imageExtent.width;
    desc.height = info->imageExtent.height;
    desc.layers = 1;
    desc.format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
    desc.usage = AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT |
                 AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;
    if (AHardwareBuffer_allocate(&desc, &i.buffer)) {
      result = Fail("AHB allocation failed", VK_ERROR_OUT_OF_DEVICE_MEMORY);
      break;
    }
    VkAndroidHardwareBufferFormatPropertiesANDROID formatProperties{
        VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID};
    VkAndroidHardwareBufferPropertiesANDROID props{
        VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID};
    props.pNext = &formatProperties;
    result = ahb(device, i.buffer, &props);
    if (result != VK_SUCCESS)
      break;
    VkExternalMemoryImageCreateInfo ext{
        VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO};
    ext.handleTypes =
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID;
    VkImageCreateInfo image{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, &ext};
    image.imageType = VK_IMAGE_TYPE_2D;
    image.format = info->imageFormat;
    // AHB stores RGBA bytes; sRGB is a compatible image view interpretation.
    // Mutable format permits the UNORM AHB backing and an sRGB render target.
    if (image.format == VK_FORMAT_R8G8B8A8_SRGB &&
        formatProperties.format == VK_FORMAT_R8G8B8A8_UNORM)
      image.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
    else if (formatProperties.format != image.format) {
      result = VK_ERROR_FORMAT_NOT_SUPPORTED;
      break;
    }
    image.extent = {desc.width, desc.height, 1};
    image.mipLevels = 1;
    image.arrayLayers = 1;
    image.samples = VK_SAMPLE_COUNT_1_BIT;
    image.tiling = VK_IMAGE_TILING_OPTIMAL;
    image.usage = info->imageUsage;
    image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    result = FN(vkCreateImage)(device, &image, nullptr, &i.image);
    if (result != VK_SUCCESS)
      break;
    VkImportAndroidHardwareBufferInfoANDROID import{
        VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID};
    import.buffer = i.buffer;
    VkMemoryDedicatedAllocateInfo dedicated{
        VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO, &import};
    dedicated.image = i.image;
    VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                               &dedicated};
    alloc.allocationSize = props.allocationSize;
    if (!props.memoryTypeBits) {
      result = VK_ERROR_FORMAT_NOT_SUPPORTED;
      break;
    }
    alloc.memoryTypeIndex = __builtin_ctz(props.memoryTypeBits);
    result = FN(vkAllocateMemory)(device, &alloc, nullptr, &i.memory);
    if (result != VK_SUCCESS)
      break;
    result = FN(vkBindImageMemory)(device, i.image, i.memory, 0);
    if (result != VK_SUCCESS)
      break;
    if (!s->connection.ImportHardwareBuffer(s->window, desc.width, desc.height,
                                            i.buffer, i.pixmap, error)) {
      result = Fail(error.c_str(), VK_ERROR_SURFACE_LOST_KHR);
      break;
    }
  }
  if (result != VK_SUCCESS) {
    Cleanup(*s);
    return result;
  }
  s->wakeFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (s->wakeFd < 0) {
    Cleanup(*s);
    return VK_ERROR_OUT_OF_HOST_MEMORY;
  }
  try {
    s->worker = std::thread(PresentWorker, std::ref(*s));
  } catch (const std::system_error &) {
    Cleanup(*s);
    return VK_ERROR_OUT_OF_HOST_MEMORY;
  }
  *out = Handle<VkSwapchainKHR>(s.get());
  {
    std::lock_guard lock(mutex);
    chains.emplace(*out, std::move(s));
  }
  return VK_SUCCESS;
}
VkImageLayout Layout(VkImageLayout v) {
  return v == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR ? VK_IMAGE_LAYOUT_GENERAL : v;
}
VkResult RenderPass(VkDevice d, const VkRenderPassCreateInfo *info,
                    const VkAllocationCallbacks *, VkRenderPass *out) {
  if (!info)
    return VK_ERROR_INITIALIZATION_FAILED;
  auto copy = *info;
  std::vector<VkAttachmentDescription> a;
  if (info->attachmentCount)
    a.assign(info->pAttachments, info->pAttachments + info->attachmentCount);
  for (auto &v : a) {
    v.initialLayout = Layout(v.initialLayout);
    v.finalLayout = Layout(v.finalLayout);
  }
  copy.pAttachments = a.data();
  return FN(vkCreateRenderPass)(d, &copy, nullptr, out);
}
void Barrier(VkCommandBuffer b, VkPipelineStageFlags src,
             VkPipelineStageFlags dst, VkDependencyFlags dep, uint32_t nm,
             const VkMemoryBarrier *m, uint32_t nb,
             const VkBufferMemoryBarrier *buffers, uint32_t ni,
             const VkImageMemoryBarrier *images) {
  std::vector<VkImageMemoryBarrier> v;
  if (ni)
    v.assign(images, images + ni);
  for (auto &i : v) {
    i.oldLayout = Layout(i.oldLayout);
    i.newLayout = Layout(i.newLayout);
  }
  FN(vkCmdPipelineBarrier)(b, src, dst, dep, nm, m, nb, buffers, ni, v.data());
}
void RegisterDevice(VkPhysicalDevice p, VkDevice d,
                    const VkDeviceCreateInfo *info) {
  uint32_t n = 0;
  FN(vkGetPhysicalDeviceQueueFamilyProperties)(p, &n, nullptr);
  std::vector<VkQueueFamilyProperties> q(n);
  FN(vkGetPhysicalDeviceQueueFamilyProperties)(p, &n, q.data());
  std::lock_guard lock(mutex);
  for (uint32_t i = 0; i < info->queueCreateInfoCount; ++i) {
    const auto &created = info->pQueueCreateInfos[i];
    auto f = created.queueFamilyIndex;
    for (uint32_t index = 0; index < created.queueCount; ++index) {
      VkQueue queue{};
      if (created.flags) {
        VkDeviceQueueInfo2 query{VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2};
        query.flags = created.flags;
        query.queueFamilyIndex = f;
        query.queueIndex = index;
        auto get = FN(vkGetDeviceQueue2);
        if (get) get(d, &query, &queue);
      } else FN(vkGetDeviceQueue)(d, f, index, &queue);
      if (queue) {
        auto state = Queue(queue);
        std::lock_guard lock(queuesMutex);
        state->device = d;
      }
    }
    if (!created.flags && !deviceFamilies.count(d) && f < n &&
        (q[f].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
      deviceFamilies[d] = f;
    }
  }
}
void DestroyDevice(VkDevice device, const VkAllocationCallbacks *alloc) {
  {
    std::lock_guard lock(mutex);
    deviceFamilies.erase(device);
  }
  {
    std::lock_guard lock(queuesMutex);
    for (auto it = queues.begin(); it != queues.end();)
      if (it->second->device == device) it = queues.erase(it);
      else ++it;
  }
  FN(vkDestroyDevice)(device, alloc);
}
void DestroySwapchain(VkDevice, VkSwapchainKHR chain,
                      const VkAllocationCallbacks *) {
  std::shared_ptr<Swapchain> s;
  {
    std::lock_guard lock(mutex);
    auto it = chains.find(chain);
    if (it == chains.end()) return;
    s = std::move(it->second);
    chains.erase(it);
  }
  {
    std::lock_guard lock(s->stateMutex);
    s->destroyed = true;
    s->changed.notify_all();
  }
  Wake(*s);
  if (s->worker.joinable()) s->worker.join();
  Cleanup(*s);
}
VkResult Images(VkDevice, VkSwapchainKHR chain, uint32_t *n, VkImage *out) {
  std::lock_guard lock(mutex);
  auto it = chains.find(chain);
  if (it == chains.end())
    return VK_ERROR_OUT_OF_DATE_KHR;
  std::vector<VkImage> v;
  for (auto &i : it->second->images)
    v.push_back(i.image);
  return Enumerate(v, n, out);
}
VkResult Acquire(VkDevice device, VkSwapchainKHR chain, uint64_t timeout,
                 VkSemaphore semaphore, VkFence fence, uint32_t *out) {
  std::shared_ptr<Swapchain> owner;
  {
    std::lock_guard lock(mutex);
    auto it = chains.find(chain);
    if (it == chains.end() || !out || it->second->device != device)
      return VK_ERROR_OUT_OF_DATE_KHR;
    owner = it->second;
  }
  auto &s = *owner;
  std::unique_lock lock(s.stateMutex);
  using Clock = std::chrono::steady_clock;
  const auto now = Clock::now();
  const auto maxNs = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::time_point::max() - now).count();
  const auto deadline = timeout == UINT64_MAX || timeout >= uint64_t(maxNs)
      ? Clock::time_point::max() : now + std::chrono::nanoseconds(timeout);
  for (;;) {
    if (s.destroyed) return VK_ERROR_OUT_OF_DATE_KHR;
    if (s.deferredError != VK_SUCCESS) return s.deferredError;
    for (uint32_t n = 0; n < s.images.size(); ++n) {
      auto &i = s.images[n];
      if (i.acquired || i.pending || i.busy)
        continue;
      auto ownership = Ownership(s, i, true);
      if (ownership != VK_SUCCESS)
        return ownership;
      VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
      submit.commandBufferCount = 1;
      submit.pCommandBuffers = &i.acquireCommand;
      // When a caller fence is supplied, signal both caller primitives in
      // the trailing batch. A failed marker must not leave its semaphore
      // signaled even though Acquire returned an error.
      submit.signalSemaphoreCount = semaphore && !fence ? 1 : 0;
      submit.pSignalSemaphores = &semaphore;
      auto queue = Queue(s.queue);
      std::lock_guard queueLock(queue->mutex);
      auto result = FN(vkResetFences)(device, 1, &i.acquireFence);
      if (result == VK_SUCCESS) {
        // Reset has removed the previous completion. If Submit fails there
        // is no outstanding use to wait for during swapchain destruction.
        i.acquireSubmitted = false;
        result = FN(vkQueueSubmit)(s.queue, 1, &submit, i.acquireFence);
      }
      if (result != VK_SUCCESS) {
        LatchError(s, result);
        return result;
      }
      i.acquireSubmitted = true;
      // A private fence tracks command-buffer lifetime even if the caller
      // destroys/reuses its fence before destroying this swapchain.
      if (fence) {
        VkSubmitInfo signal{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        signal.signalSemaphoreCount = semaphore ? 1 : 0;
        signal.pSignalSemaphores = &semaphore;
        result = FN(vkQueueSubmit)(s.queue, 1, &signal, fence);
        if (result != VK_SUCCESS) {
          LatchError(s, result);
          return result;
        }
      }
      i.acquired = true;
      i.initialized = true;
      *out = n;
      return VK_SUCCESS;
    }
    if (!timeout) return VK_NOT_READY;
    if (deadline == Clock::time_point::max()) s.changed.wait(lock);
    else if (s.changed.wait_until(lock, deadline) == std::cv_status::timeout) {
      // Recheck state at the deadline: an IdleNotify may have raced expiry.
      if (s.deferredError != VK_SUCCESS) return s.deferredError;
      if (s.destroyed) return VK_ERROR_OUT_OF_DATE_KHR;
      const bool available = std::any_of(s.images.begin(), s.images.end(),
          [](const Image &i) { return !i.acquired && !i.pending && !i.busy; });
      if (!available) return VK_TIMEOUT;
    }
  }
}
VkResult Acquire2(VkDevice device, const VkAcquireNextImageInfoKHR *info,
                  uint32_t *out) {
  if (!info || info->sType != VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR ||
      info->pNext || info->deviceMask != 1)
    return Fail("Unsupported AcquireNextImage2 parameters");
  return Acquire(device, info->swapchain, info->timeout, info->semaphore,
                 info->fence, out);
}
VkResult Caps2(VkPhysicalDevice physicalDevice,
               const VkPhysicalDeviceSurfaceInfo2KHR *info,
               VkSurfaceCapabilities2KHR *out) {
  if (!info || !out ||
      info->sType != VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR ||
      out->sType != VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR)
    return VK_ERROR_INITIALIZATION_FAILED;
  if (info->pNext || out->pNext)
    return Fail("SurfaceCapabilities2 extension chain unsupported");
  return Caps(physicalDevice, info->surface, &out->surfaceCapabilities);
}
VkResult Formats2(VkPhysicalDevice,
                  const VkPhysicalDeviceSurfaceInfo2KHR *info, uint32_t *count,
                  VkSurfaceFormat2KHR *out) {
  if (!info || !count ||
      info->sType != VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR)
    return VK_ERROR_INITIALIZATION_FAILED;
  if (info->pNext)
    return Fail("SurfaceFormats2 input extension chain unsupported");
  std::lock_guard lock(mutex);
  if (!surfaces.count(info->surface))
    return VK_ERROR_SURFACE_LOST_KHR;
  constexpr VkSurfaceFormatKHR formats[] = {
      {VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
      {VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};
  if (!out) {
    *count = std::size(formats);
    return VK_SUCCESS;
  }
  uint32_t written = std::min(*count, uint32_t(std::size(formats)));
  for (uint32_t n = 0; n < written; ++n) {
    if (out[n].sType != VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR)
      return VK_ERROR_INITIALIZATION_FAILED;
    if (out[n].pNext)
      return Fail("SurfaceFormats2 output extension chain unsupported");
    out[n].surfaceFormat = formats[n];
  }
  *count = written;
  return written < std::size(formats) ? VK_INCOMPLETE : VK_SUCCESS;
}
VkResult Present(VkQueue queue, const VkPresentInfoKHR *info) {
  std::lock_guard lock(mutex);
  if (!info || info->pNext)
    return Fail("Present pNext unsupported");
  if (!info->swapchainCount || !info->pSwapchains || !info->pImageIndices ||
      (info->waitSemaphoreCount && !info->pWaitSemaphores))
    return Fail("Invalid Present arguments", VK_ERROR_INITIALIZATION_FAILED);
  std::vector<VkPipelineStageFlags> stages(info->waitSemaphoreCount,
                                           VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
  // Phase 1: validate each entry and record its release command. Validation
  // results are written to pResults immediately; the X11 handoff happens later
  // on the worker and deferred errors surface on the next Acquire/Present.
  std::vector<VkCommandBuffer> commands;
  std::vector<std::tuple<Swapchain *, uint32_t, uint32_t>> accepted;
  std::vector<Swapchain *> lockedChains;
  std::vector<std::unique_lock<std::mutex>> chainLocks;
  VkResult result = VK_SUCCESS;
  for (uint32_t n = 0; n < info->swapchainCount; ++n) {
    auto it = chains.find(info->pSwapchains[n]);
    VkResult r = VK_ERROR_OUT_OF_DATE_KHR;
    if (it != chains.end()) {
      auto &s = *it->second;
      if (std::find(lockedChains.begin(), lockedChains.end(), &s) != lockedChains.end())
        return Fail("Duplicate present swapchain", VK_ERROR_INITIALIZATION_FAILED);
      lockedChains.push_back(&s);
      chainLocks.emplace_back(s.stateMutex);
      uint32_t index = info->pImageIndices[n];
      if (s.deferredError != VK_SUCCESS) {
        r = s.deferredError;
      } else if (s.destroyed) {
        r = VK_ERROR_OUT_OF_DATE_KHR;
      } else if (queue != s.queue || index >= s.images.size() ||
                 !s.images[index].acquired || s.images[index].pending) {
        r = Fail("Present queue/image not acquired by this swapchain");
      } else {
        r = Ownership(s, s.images[index], false);
        if (r == VK_SUCCESS) {
          commands.push_back(s.images[index].releaseCommand);
          accepted.emplace_back(&s, index, n);
        }
      }
    }
    if (info->pResults)
      info->pResults[n] = r;
    if (r != VK_SUCCESS)
      result = r;
  }
  if (accepted.empty())
    return result;
  // Phase 2: one submission for every accepted image. A single submit consumes
  // the caller's wait semaphores exactly once, which binary semaphores
  // require, and keeps VkQueue ordering on the caller's thread.
  const VkDevice device = std::get<0>(accepted.front())->device;
  std::shared_ptr<PresentFence> completion;
  VkResult r;
  if (accepted.size() == 1) {
    completion =
        std::get<0>(accepted.front())->images[std::get<1>(accepted.front())]
            .presentFence;
    r = FN(vkResetFences)(device, 1, &completion->fence);
  } else {
    completion = std::make_shared<PresentFence>();
    completion->device = device;
    VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    r = FN(vkCreateFence)(device, &fi, nullptr, &completion->fence);
  }
  if (r == VK_SUCCESS) {
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = commands.size();
    submit.pCommandBuffers = commands.data();
    submit.waitSemaphoreCount = info->waitSemaphoreCount;
    submit.pWaitSemaphores = info->pWaitSemaphores;
    submit.pWaitDstStageMask = stages.data();
    r = Submit(queue, 1, &submit, completion->fence);
  }
  if (r != VK_SUCCESS) {
    // A submission failure applies to every accepted entry. The images stay
    // acquired so the caller can retry presenting them.
    if (info->pResults)
      for (auto &a : accepted)
        info->pResults[std::get<2>(a)] = r;
    if (r == VK_ERROR_DEVICE_LOST)
      for (auto &[s, index, entry] : accepted) LatchError(*s, r);
    return r;
  }
  // Phase 3: mark and enqueue. Serials are assigned here so the worker FIFO
  // preserves each swapchain's order. Shared completion owns a multi-image
  // submission's fence until every affected worker has finished waiting.
  for (auto &[s, index, entry] : accepted) {
    auto &i = s->images[index];
    i.acquired = false;
    i.pending = true;
    s->jobs.push_back({completion, index, ++s->serial});
    Wake(*s);
  }
  return result;
}
} // namespace
extern "C" void *Vexa_X11VulkanGetProcAddr(const char *name) {
#define ENTRY(n, f)                                                            \
  if (!strcmp(name, n))                                                        \
  return reinterpret_cast<void *>(f)
  ENTRY("vkAcquireNextImage2KHR", Acquire2);
  ENTRY("vkGetPhysicalDeviceSurfaceCapabilities2KHR", Caps2);
  ENTRY("vkGetPhysicalDeviceSurfaceFormats2KHR", Formats2);
  ENTRY("VexaRegisterX11Device", RegisterDevice);
  ENTRY("vkCreateRenderPass", RenderPass);
  ENTRY("vkCmdPipelineBarrier", Barrier);
  ENTRY("VexaCreateX11Surface", CreateSurface);
  ENTRY("vkDestroySurfaceKHR", DestroySurface);
  ENTRY("vkGetPhysicalDeviceSurfaceSupportKHR", Support);
  ENTRY("vkGetPhysicalDeviceSurfaceCapabilitiesKHR", Caps);
  ENTRY("vkGetPhysicalDeviceSurfaceFormatsKHR", Formats);
  ENTRY("vkGetPhysicalDeviceSurfacePresentModesKHR", Modes);
  ENTRY("vkCreateSwapchainKHR", CreateSwapchain);
  ENTRY("vkDestroySwapchainKHR", DestroySwapchain);
  ENTRY("vkGetSwapchainImagesKHR", Images);
  ENTRY("vkAcquireNextImageKHR", Acquire);
  ENTRY("vkQueuePresentKHR", Present);
  ENTRY("vkQueueSubmit", Submit);
  ENTRY("vkQueueSubmit2", Submit2);
  ENTRY("vkQueueSubmit2KHR", Submit2KHR);
  ENTRY("vkQueueBindSparse", BindSparse);
  ENTRY("vkQueueWaitIdle", QueueIdle);
  ENTRY("vkDeviceWaitIdle", DeviceIdle);
  ENTRY("vkDestroyDevice", DestroyDevice);
  return nullptr;
}
