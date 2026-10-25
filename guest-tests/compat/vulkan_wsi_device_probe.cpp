// Actual Android ICD + Termux:X11 integration. Link with the production WSI and
// connection sources; run as the app UID while a headless X server is active.
#include <vulkan/vulkan.h>
#include <cassert>
#include <cstdio>
#include <thread>
#include <chrono>
#include <vector>
#include "../../app/src/main/cpp/x11/vulkan_wsi.h"
#include "../../app/src/main/cpp/x11/x11_present.h"

#define CHECK(call) do { VkResult r = (call); if (r != VK_SUCCESS) { fprintf(stderr, "%s -> %d at %d\n", #call, r, __LINE__); return 1; } } while (0)
#define BRIDGE(name) ([] { auto fn = reinterpret_cast<PFN_##name>(Vexa_X11VulkanGetProcAddr(#name)); return fn ? fn : &name; }())
int main(int argc, char **argv) {
  if (argc != 2) return 2;
  setenv("VEXA_X11_SOCKET", argv[1], 1);
  VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO}; app.apiVersion = VK_API_VERSION_1_1;
  VkInstanceCreateInfo instanceInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; instanceInfo.pApplicationInfo = &app;
  VkInstance instance{}; CHECK(vkCreateInstance(&instanceInfo, nullptr, &instance));
  uint32_t count = 0; CHECK(vkEnumeratePhysicalDevices(instance, &count, nullptr));
  std::vector<VkPhysicalDevice> physical(count); CHECK(vkEnumeratePhysicalDevices(instance, &count, physical.data()));
  assert(count);
  vkGetPhysicalDeviceQueueFamilyProperties(physical[0], &count, nullptr);
  std::vector<VkQueueFamilyProperties> families(count); vkGetPhysicalDeviceQueueFamilyProperties(physical[0], &count, families.data());
  uint32_t family = 0; while (!(families[family].queueFlags & VK_QUEUE_GRAPHICS_BIT)) ++family;
  const float priority = 1;
  VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
  queueInfo.queueFamilyIndex = family; queueInfo.queueCount = 1; queueInfo.pQueuePriorities = &priority;
  const char *extensions[] = {"VK_ANDROID_external_memory_android_hardware_buffer", "VK_EXT_queue_family_foreign"};
  VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
  deviceInfo.queueCreateInfoCount = 1; deviceInfo.pQueueCreateInfos = &queueInfo;
  deviceInfo.enabledExtensionCount = 2; deviceInfo.ppEnabledExtensionNames = extensions;
  VkDevice device{}; CHECK(vkCreateDevice(physical[0], &deviceInfo, nullptr, &device));
  using Register = void (*)(VkPhysicalDevice, VkDevice, const VkDeviceCreateInfo*);
  reinterpret_cast<Register>(Vexa_X11VulkanGetProcAddr("VexaRegisterX11Device"))(physical[0], device, &deviceInfo);
  VkQueue queue{}; vkGetDeviceQueue(device, family, 0, &queue);
  Vexa::X11::PresentConnection windowConnection;
  std::string error;
  if (!windowConnection.Connect(argv[1], error)) { fprintf(stderr, "%s\n", error.c_str()); return 1; }
  const uint32_t window = windowConnection.CreateWindow(640, 480, error);
  if (!window) { fprintf(stderr, "%s\n", error.c_str()); return 1; }
  using Surface = VkResult (*)(VkInstance, uint32_t, VkSurfaceKHR*);
  VkSurfaceKHR surface{};
  CHECK(reinterpret_cast<Surface>(Vexa_X11VulkanGetProcAddr("VexaCreateX11Surface"))(instance, window, &surface));
  VkSwapchainCreateInfoKHR create{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
  create.surface = surface; create.minImageCount = 2;
  create.imageFormat = VK_FORMAT_R8G8B8A8_UNORM; create.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
  create.imageExtent = {640, 480}; create.imageArrayLayers = 1;
  create.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  create.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; create.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  create.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; create.presentMode = VK_PRESENT_MODE_FIFO_KHR;
  VkSwapchainKHR chain{}; CHECK(BRIDGE(vkCreateSwapchainKHR)(device, &create, nullptr, &chain));
  count = 0; CHECK(BRIDGE(vkGetSwapchainImagesKHR)(device, chain, &count, nullptr));
  std::vector<VkImage> images(count); CHECK(BRIDGE(vkGetSwapchainImagesKHR)(device, chain, &count, images.data()));
  VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; poolInfo.queueFamilyIndex = family;
  VkCommandPool pool{}; CHECK(vkCreateCommandPool(device, &poolInfo, nullptr, &pool));
  VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  allocate.commandPool = pool; allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; allocate.commandBufferCount = count;
  std::vector<VkCommandBuffer> commands(count); CHECK(vkAllocateCommandBuffers(device, &allocate, commands.data()));
  std::vector<VkSemaphore> done(count);
  VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  for (uint32_t n = 0; n < count; ++n) {
    CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &done[n]));
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; CHECK(vkBeginCommandBuffer(commands[n], &begin));
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = images[n]; barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL; barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(commands[n], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
    VkClearColorValue color{{0.1f, 0.65f, 0.25f, 1.0f}};
    vkCmdClearColorImage(commands[n], images[n], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1, &barrier.subresourceRange);
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    vkCmdPipelineBarrier(commands[n], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
    CHECK(vkEndCommandBuffer(commands[n]));
  }
  VkSemaphore acquired[2]{}; VkFence rendered[2]{}, acquireCompleted[2]{};
  VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO}; fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
  for (unsigned n = 0; n < 2; ++n) {
    CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &acquired[n]));
    CHECK(vkCreateFence(device, &fenceInfo, nullptr, &rendered[n]));
    VkFenceCreateInfo unsignaled{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    CHECK(vkCreateFence(device, &unsignaled, nullptr, &acquireCompleted[n]));
  }
  for (unsigned frame = 0; frame < 120; ++frame) {
    const auto slot = frame % 2;
    CHECK(vkWaitForFences(device, 1, &rendered[slot], VK_TRUE, UINT64_MAX));
    CHECK(vkResetFences(device, 1, &rendered[slot]));
    if (frame >= 2) CHECK(vkResetFences(device, 1, &acquireCompleted[slot]));
    uint32_t index = 0;
    CHECK(BRIDGE(vkAcquireNextImageKHR)(device, chain, UINT64_MAX, acquired[slot], acquireCompleted[slot], &index));
    CHECK(vkWaitForFences(device, 1, &acquireCompleted[slot], VK_TRUE, UINT64_MAX));
    const VkPipelineStageFlags stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.waitSemaphoreCount = 1; submit.pWaitSemaphores = &acquired[slot]; submit.pWaitDstStageMask = &stage;
    submit.commandBufferCount = 1; submit.pCommandBuffers = &commands[index];
    submit.signalSemaphoreCount = 1; submit.pSignalSemaphores = &done[index];
    CHECK(BRIDGE(vkQueueSubmit)(queue, 1, &submit, rendered[slot]));
    VkResult perImage = VK_ERROR_UNKNOWN;
    VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present.waitSemaphoreCount = 1; present.pWaitSemaphores = &done[index];
    present.swapchainCount = 1; present.pSwapchains = &chain; present.pImageIndices = &index; present.pResults = &perImage;
    CHECK(BRIDGE(vkQueuePresentKHR)(queue, &present)); CHECK(perImage);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  CHECK(vkWaitForFences(device, 2, rendered, VK_TRUE, UINT64_MAX));
  for (auto fence : acquireCompleted) vkDestroyFence(device, fence, nullptr);
  BRIDGE(vkDestroySwapchainKHR)(device, chain, nullptr);
  puts("PASS actual Mali ICD: 120 two-image Acquire semaphore+fence/render/async Present cycles; caller fences destroyed before swapchain"); fflush(stdout);
  std::this_thread::sleep_for(std::chrono::seconds(3));
  for (auto semaphore : done) vkDestroySemaphore(device, semaphore, nullptr);
  for (unsigned n = 0; n < 2; ++n) { vkDestroySemaphore(device, acquired[n], nullptr); vkDestroyFence(device, rendered[n], nullptr); }
  vkDestroyCommandPool(device, pool, nullptr);
  BRIDGE(vkDestroySurfaceKHR)(instance, surface, nullptr);
  BRIDGE(vkDestroyDevice)(device, nullptr);
  vkDestroyInstance(instance, nullptr);
}
