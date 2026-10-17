#pragma once
#include <cstdint>
// Host Vulkan ABI only. Returned functions are never guest-callable pointers.
extern "C" __attribute__((visibility("default"))) void *
Vexa_X11VulkanGetProcAddr(const char *name);
