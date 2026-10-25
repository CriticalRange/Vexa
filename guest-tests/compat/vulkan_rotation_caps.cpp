#include <vulkan/vulkan.h>
#include <cstdio>
#include <cstring>
#include <vector>
int main() {
  VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO}; app.apiVersion = VK_API_VERSION_1_1;
  VkInstanceCreateInfo create{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; create.pApplicationInfo = &app;
  VkInstance instance{};
  if (vkCreateInstance(&create, nullptr, &instance) != VK_SUCCESS) return 1;
  uint32_t count = 0; vkEnumeratePhysicalDevices(instance, &count, nullptr);
  std::vector<VkPhysicalDevice> devices(count); vkEnumeratePhysicalDevices(instance, &count, devices.data());
  for (auto device : devices) {
    VkPhysicalDeviceProperties props{}; vkGetPhysicalDeviceProperties(device, &props);
    printf("device=%s\n", props.deviceName);
    uint32_t n = 0; vkEnumerateDeviceExtensionProperties(device, nullptr, &n, nullptr);
    std::vector<VkExtensionProperties> extensions(n); vkEnumerateDeviceExtensionProperties(device, nullptr, &n, extensions.data());
    bool transform = false;
    for (auto &e : extensions) {
      if (!strcmp(e.extensionName, "VK_QCOM_render_pass_transform")) transform = true;
      if (strstr(e.extensionName, "present") || strstr(e.extensionName, "transform") || strstr(e.extensionName, "compression") || strstr(e.extensionName, "timeline"))
        printf("extension=%s\n", e.extensionName);
    }
    printf("VK_QCOM_render_pass_transform=%s\n", transform ? "supported" : "absent");
    VkPhysicalDeviceTimelineSemaphoreFeatures timeline{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES};
    VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2}; features.pNext = &timeline;
    vkGetPhysicalDeviceFeatures2(device, &features);
    printf("timelineSemaphore=%u\n", timeline.timelineSemaphore);
  }
  vkDestroyInstance(instance, nullptr);
}
