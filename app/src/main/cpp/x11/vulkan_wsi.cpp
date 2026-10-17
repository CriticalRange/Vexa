#define VK_USE_PLATFORM_ANDROID_KHR
#include "vulkan_wsi.h"
#include "x11_present.h"
#include <algorithm>
#include <android/hardware_buffer.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <iterator>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

namespace {
// Synthetic handles are private to this WSI. Never pass them to the Android
// ICD.
struct Surface {
  uint32_t window;
};
struct Image {
  VkImage image{};
  VkDeviceMemory memory{};
  AHardwareBuffer *buffer{};
  uint32_t pixmap{};
  bool acquired = false, busy = false, initialized = false;
  VkCommandBuffer acquireCommand{}, releaseCommand{};
};
struct Swapchain {
  VkDevice device;
  VkQueue queue;
  uint32_t family;
  VkCommandPool pool{};
  VkFence presentFence{};
  uint32_t window, serial = 0;
  Vexa::X11::PresentConnection connection;
  std::vector<Image> images;
};
std::mutex mutex;
std::unordered_map<VkDevice, uint32_t> deviceFamilies;
std::unordered_map<VkSurfaceKHR, std::unique_ptr<Surface>> surfaces;
std::unordered_map<VkSwapchainKHR, std::unique_ptr<Swapchain>> chains;
void *library = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
template <typename T> T Load(const char *name) {
  return reinterpret_cast<T>(dlsym(library, name));
}
#define FN(n) Load<PFN_##n>(#n)
template <class H, class P> H Handle(P *p) { return (H)(uintptr_t)p; }
VkResult Fail(const char *why, VkResult code = VK_ERROR_FEATURE_NOT_PRESENT) {
  fprintf(stderr, "[VEXA][X11-WSI] %s\n", why);
  return code;
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
  FN(vkDeviceWaitIdle)(s.device);
  std::string error;
  if (s.presentFence)
    FN(vkDestroyFence)(s.device, s.presentFence, nullptr);
  if (s.pool)
    FN(vkDestroyCommandPool)(s.device, s.pool, nullptr);
  for (auto &i : s.images) {
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
}
VkResult CreateSwapchain(VkDevice device, const VkSwapchainCreateInfoKHR *info,
                         const VkAllocationCallbacks *, VkSwapchainKHR *out) {
  std::lock_guard lock(mutex);
  if (!info || !out || !surfaces.count(info->surface))
    return VK_ERROR_SURFACE_LOST_KHR;
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
  auto s = std::make_unique<Swapchain>();
  s->device = device;
  s->window = surfaces.at(info->surface)->window;
  if (!deviceFamilies.count(device))
    return Fail("No graphics queue registered");
  s->family = deviceFamilies.at(device);
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
  result = FN(vkCreateFence)(device, &fenceInfo, nullptr, &s->presentFence);
  if (result != VK_SUCCESS) {
    Cleanup(*s);
    return result;
  }
  for (uint32_t n = 0; n < info->minImageCount; ++n) {
    s->images.emplace_back();
    auto &i = s->images.back();
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
  *out = Handle<VkSwapchainKHR>(s.get());
  chains.emplace(*out, std::move(s));
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
    auto f = info->pQueueCreateInfos[i].queueFamilyIndex;
    if (f < n && (q[f].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
      deviceFamilies[d] = f;
      return;
    }
  }
}
void DestroySwapchain(VkDevice, VkSwapchainKHR chain,
                      const VkAllocationCallbacks *) {
  std::lock_guard lock(mutex);
  auto it = chains.find(chain);
  if (it != chains.end()) {
    Cleanup(*it->second);
    chains.erase(it);
  }
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
  std::lock_guard lock(mutex);
  auto it = chains.find(chain);
  if (it == chains.end() || !out || it->second->device != device)
    return VK_ERROR_OUT_OF_DATE_KHR;
  auto &s = *it->second;
  std::string error;
  using Clock = std::chrono::steady_clock;
  const auto start = Clock::now();
  // Poll every buffer before blocking on one. One timeout budget covers the
  // call.
  for (unsigned pass = 0; pass < 2; ++pass)
    for (uint32_t n = 0; n < s.images.size(); ++n) {
      auto &i = s.images[n];
      if (i.acquired)
        continue;
      uint64_t left = 0;
      if (pass) {
        auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
                           Clock::now() - start)
                           .count();
        left = timeout == UINT64_MAX
                   ? UINT64_MAX
                   : (timeout > uint64_t(elapsed) ? timeout - elapsed : 0);
      }
      if (i.busy && !s.connection.WaitIdle(i.pixmap, left, error))
        continue;
      i.busy = false;
      auto ownership = Ownership(s, i, true);
      if (ownership != VK_SUCCESS)
        return ownership;
      VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
      submit.commandBufferCount = 1;
      submit.pCommandBuffers = &i.acquireCommand;
      submit.signalSemaphoreCount = semaphore ? 1 : 0;
      submit.pSignalSemaphores = &semaphore;
      auto result = FN(vkQueueSubmit)(s.queue, 1, &submit, fence);
      if (result != VK_SUCCESS)
        return result;
      i.acquired = true;
      i.initialized = true;
      *out = n;
      return VK_SUCCESS;
    }
  return timeout ? VK_TIMEOUT : VK_NOT_READY;
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
  std::vector<VkCommandBuffer> commands;
  VkDevice device = VK_NULL_HANDLE;
  VkFence fence = VK_NULL_HANDLE;
  for (uint32_t n = 0; n < info->swapchainCount; ++n) {
    auto it = chains.find(info->pSwapchains[n]);
    if (it == chains.end())
      return VK_ERROR_OUT_OF_DATE_KHR;
    auto &s = *it->second;
    if (!device) {
      device = s.device;
      fence = s.presentFence;
    } else if (device != s.device) {
      return Fail("Present swapchains use different devices",
                  VK_ERROR_INITIALIZATION_FAILED);
    }
    auto index = info->pImageIndices[n];
    if (queue != s.queue || index >= s.images.size() ||
        !s.images[index].acquired)
      return Fail("Present queue/image not acquired by this swapchain");
    auto result = Ownership(s, s.images[index], false);
    if (result != VK_SUCCESS)
      return result;
    commands.push_back(s.images[index].releaseCommand);
  }
  std::vector<VkPipelineStageFlags> stages(info->waitSemaphoreCount,
                                           VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
  VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submit.commandBufferCount = commands.size();
  submit.pCommandBuffers = commands.data();
  submit.waitSemaphoreCount = info->waitSemaphoreCount;
  submit.pWaitSemaphores = info->pWaitSemaphores;
  submit.pWaitDstStageMask = stages.data();
  // Waiting for this release submission is enough to hand the AHB to X11.
  // QueueWaitIdle also waits for unrelated work queued by the game.
  VkResult result = FN(vkResetFences)(device, 1, &fence);
  if (result != VK_SUCCESS)
    return result;
  result = FN(vkQueueSubmit)(queue, 1, &submit, fence);
  if (result != VK_SUCCESS)
    return result;
  result = FN(vkWaitForFences)(device, 1, &fence, VK_TRUE, UINT64_MAX);
  if (result != VK_SUCCESS)
    return result;
  std::string error;
  for (uint32_t n = 0; n < info->swapchainCount; ++n) {
    auto it = chains.find(info->pSwapchains[n]);
    VkResult r = VK_ERROR_OUT_OF_DATE_KHR;
    if (it != chains.end()) {
      auto &s = *it->second;
      uint32_t index = info->pImageIndices[n];
      if (index < s.images.size() && s.images[index].acquired) {
        auto &i = s.images[index];
        r = s.connection.Present(s.window, i.pixmap, ++s.serial, error)
                ? VK_SUCCESS
                : Fail(error.c_str(), VK_ERROR_SURFACE_LOST_KHR);
        if (r == VK_SUCCESS) {
          i.acquired = false;
          i.busy = true;
        }
      }
    }
    if (info->pResults)
      info->pResults[n] = r;
    if (r != VK_SUCCESS)
      result = r;
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
  return nullptr;
}
