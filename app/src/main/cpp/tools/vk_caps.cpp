// Dumps the device's Vulkan capabilities as JSON on stdout: versions, driver,
// limits, extensions, core/extension features, memory heaps and the format
// support relevant to D3D translation. tools/vulkan_caps.py runs this over adb
// and compares the result against a DXVK requirement profile.
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include <dlfcn.h>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace {

std::string Escape(const char* s) {
    std::string out;
    for (; *s; ++s) {
        const unsigned char c = *s;
        if (c == '"' || c == '\\') { out += '\\'; out += char(c); }
        else if (c < 0x20) { char buf[8]; snprintf(buf, sizeof(buf), "\\u%04x", c); out += buf; }
        else out += char(c);
    }
    return out;
}

std::string Version(uint32_t v) {
    return std::to_string(VK_API_VERSION_MAJOR(v)) + "." + std::to_string(VK_API_VERSION_MINOR(v)) + "." +
           std::to_string(VK_API_VERSION_PATCH(v));
}

// Writes `"key":value` pairs separated by commas inside one object.
struct Obj {
    bool first = true;
    void Key(const char* k) { printf("%s\"%s\":", first ? "" : ",", k); first = false; }
    void Bool(const char* k, VkBool32 v) { Key(k); printf(v ? "true" : "false"); }
    void Num(const char* k, double v) { Key(k); printf("%.17g", v); }
    void Str(const char* k, const std::string& v) { Key(k); printf("\"%s\"", Escape(v.c_str()).c_str()); }
    void Hex(const char* k, uint64_t v) { Key(k); printf("\"0x%llx\"", (unsigned long long)v); }
    void Open(const char* k) { Key(k); printf("{"); }
    void Close() { printf("}"); }
};

#define CORE_FEATURES(X) X(robustBufferAccess) X(fullDrawIndexUint32) X(imageCubeArray) X(independentBlend) X(geometryShader) \
    X(tessellationShader) X(sampleRateShading) X(dualSrcBlend) X(logicOp) X(multiDrawIndirect) X(drawIndirectFirstInstance) \
    X(depthClamp) X(depthBiasClamp) X(fillModeNonSolid) X(depthBounds) X(wideLines) X(largePoints) X(alphaToOne) \
    X(multiViewport) X(samplerAnisotropy) X(textureCompressionETC2) X(textureCompressionASTC_LDR) X(textureCompressionBC) \
    X(occlusionQueryPrecise) X(pipelineStatisticsQuery) X(vertexPipelineStoresAndAtomics) X(fragmentStoresAndAtomics) \
    X(shaderTessellationAndGeometryPointSize) X(shaderImageGatherExtended) X(shaderStorageImageExtendedFormats) \
    X(shaderStorageImageMultisample) X(shaderStorageImageReadWithoutFormat) X(shaderStorageImageWriteWithoutFormat) \
    X(shaderUniformBufferArrayDynamicIndexing) X(shaderSampledImageArrayDynamicIndexing) \
    X(shaderStorageBufferArrayDynamicIndexing) X(shaderStorageImageArrayDynamicIndexing) X(shaderClipDistance) \
    X(shaderCullDistance) X(shaderFloat64) X(shaderInt64) X(shaderInt16) X(shaderResourceResidency) X(shaderResourceMinLod) \
    X(sparseBinding) X(sparseResidencyBuffer) X(sparseResidencyImage2D) X(sparseResidencyImage3D) X(sparseResidency2Samples) \
    X(sparseResidency4Samples) X(sparseResidency8Samples) X(sparseResidency16Samples) X(sparseResidencyAliased) \
    X(variableMultisampleRate) X(inheritedQueries)

#define VK11_FEATURES(X) X(storageBuffer16BitAccess) X(uniformAndStorageBuffer16BitAccess) X(storagePushConstant16) \
    X(storageInputOutput16) X(multiview) X(multiviewGeometryShader) X(multiviewTessellationShader) \
    X(variablePointersStorageBuffer) X(variablePointers) X(protectedMemory) X(samplerYcbcrConversion) X(shaderDrawParameters)

#define VK12_FEATURES(X) X(samplerMirrorClampToEdge) X(drawIndirectCount) X(storageBuffer8BitAccess) \
    X(uniformAndStorageBuffer8BitAccess) X(storagePushConstant8) X(shaderBufferInt64Atomics) X(shaderSharedInt64Atomics) \
    X(shaderFloat16) X(shaderInt8) X(descriptorIndexing) X(shaderInputAttachmentArrayDynamicIndexing) \
    X(shaderUniformTexelBufferArrayDynamicIndexing) X(shaderStorageTexelBufferArrayDynamicIndexing) \
    X(shaderUniformBufferArrayNonUniformIndexing) X(shaderSampledImageArrayNonUniformIndexing) \
    X(shaderStorageBufferArrayNonUniformIndexing) X(shaderStorageImageArrayNonUniformIndexing) \
    X(shaderInputAttachmentArrayNonUniformIndexing) X(shaderUniformTexelBufferArrayNonUniformIndexing) \
    X(shaderStorageTexelBufferArrayNonUniformIndexing) X(descriptorBindingUniformBufferUpdateAfterBind) \
    X(descriptorBindingSampledImageUpdateAfterBind) X(descriptorBindingStorageImageUpdateAfterBind) \
    X(descriptorBindingStorageBufferUpdateAfterBind) X(descriptorBindingUniformTexelBufferUpdateAfterBind) \
    X(descriptorBindingStorageTexelBufferUpdateAfterBind) X(descriptorBindingUpdateUnusedWhilePending) \
    X(descriptorBindingPartiallyBound) X(descriptorBindingVariableDescriptorCount) X(runtimeDescriptorArray) \
    X(samplerFilterMinmax) X(scalarBlockLayout) X(imagelessFramebuffer) X(uniformBufferStandardLayout) \
    X(shaderSubgroupExtendedTypes) X(separateDepthStencilLayouts) X(hostQueryReset) X(timelineSemaphore) \
    X(bufferDeviceAddress) X(bufferDeviceAddressCaptureReplay) X(bufferDeviceAddressMultiDevice) X(vulkanMemoryModel) \
    X(vulkanMemoryModelDeviceScope) X(vulkanMemoryModelAvailabilityVisibilityChains) X(shaderOutputViewportIndex) \
    X(shaderOutputLayer) X(subgroupBroadcastDynamicId)

#define VK13_FEATURES(X) X(robustImageAccess) X(inlineUniformBlock) X(descriptorBindingInlineUniformBlockUpdateAfterBind) \
    X(pipelineCreationCacheControl) X(privateData) X(shaderDemoteToHelperInvocation) X(shaderTerminateInvocation) \
    X(subgroupSizeControl) X(computeFullSubgroups) X(synchronization2) X(textureCompressionASTC_HDR) \
    X(shaderZeroInitializeWorkgroupMemory) X(dynamicRendering) X(shaderIntegerDotProduct) X(maintenance4)

#define VK14_FEATURES(X) X(globalPriorityQuery) X(shaderSubgroupRotate) X(shaderSubgroupRotateClustered) \
    X(shaderFloatControls2) X(shaderExpectAssume) X(rectangularLines) X(bresenhamLines) X(smoothLines) \
    X(stippledRectangularLines) X(stippledBresenhamLines) X(stippledSmoothLines) X(vertexAttributeInstanceRateDivisor) \
    X(vertexAttributeInstanceRateZeroDivisor) X(indexTypeUint8) X(dynamicRenderingLocalRead) X(maintenance5) X(maintenance6) \
    X(pipelineProtectedAccess) X(pipelineRobustness) X(hostImageCopy) X(pushDescriptor)

#define LIMITS(X) X(maxImageDimension2D) X(maxImageDimension3D) X(maxImageDimensionCube) X(maxImageArrayLayers) \
    X(maxTexelBufferElements) X(maxUniformBufferRange) X(maxStorageBufferRange) X(maxPushConstantsSize) \
    X(maxMemoryAllocationCount) X(maxSamplerAllocationCount) X(maxBoundDescriptorSets) X(maxPerStageDescriptorSamplers) \
    X(maxPerStageDescriptorUniformBuffers) X(maxPerStageDescriptorStorageBuffers) X(maxPerStageDescriptorSampledImages) \
    X(maxPerStageDescriptorStorageImages) X(maxPerStageResources) X(maxDescriptorSetUniformBuffersDynamic) \
    X(maxDescriptorSetStorageBuffersDynamic) X(maxVertexInputAttributes) X(maxVertexInputBindings) \
    X(maxVertexInputBindingStride) X(maxGeometryOutputVertices) X(maxComputeSharedMemorySize) X(maxColorAttachments) \
    X(maxSamplerAnisotropy) X(maxViewports) X(minTexelBufferOffsetAlignment) X(minUniformBufferOffsetAlignment) \
    X(minStorageBufferOffsetAlignment) X(maxClipDistances) X(maxCullDistances) X(timestampPeriod) \
    X(framebufferColorSampleCounts) X(framebufferDepthSampleCounts) X(nonCoherentAtomSize)

#define FORMATS(X) X(BC1_RGB_UNORM_BLOCK) X(BC1_RGBA_UNORM_BLOCK) X(BC1_RGBA_SRGB_BLOCK) X(BC2_UNORM_BLOCK) X(BC2_SRGB_BLOCK) \
    X(BC3_UNORM_BLOCK) X(BC3_SRGB_BLOCK) X(BC4_UNORM_BLOCK) X(BC5_UNORM_BLOCK) X(BC6H_UFLOAT_BLOCK) X(BC7_UNORM_BLOCK) \
    X(BC7_SRGB_BLOCK) X(D16_UNORM) X(X8_D24_UNORM_PACK32) X(D24_UNORM_S8_UINT) X(D32_SFLOAT) X(D32_SFLOAT_S8_UINT) \
    X(R8G8B8A8_UNORM) X(R8G8B8A8_SRGB) X(B8G8R8A8_UNORM) X(B8G8R8A8_SRGB) X(A2B10G10R10_UNORM_PACK32) \
    X(A2R10G10B10_UNORM_PACK32) X(B10G11R11_UFLOAT_PACK32) X(E5B9G9R9_UFLOAT_PACK32) X(R16G16B16A16_SFLOAT) \
    X(R16G16B16A16_UNORM) X(R32G32B32_SFLOAT) X(B5G6R5_UNORM_PACK16) X(B5G5R5A1_UNORM_PACK16) X(A1R5G5B5_UNORM_PACK16) \
    X(B4G4R4A4_UNORM_PACK16) X(A4R4G4B4_UNORM_PACK16)

#define ROBUSTNESS2_FEATURES(X) X(robustBufferAccess2) X(robustImageAccess2) X(nullDescriptor)
#define XFB_FEATURES(X) X(transformFeedback) X(geometryStreams)
#define DEPTH_CLIP_FEATURES(X) X(depthClipEnable)
#define MAINT5_FEATURES(X) X(maintenance5)
#define MAINT6_FEATURES(X) X(maintenance6)
#define GPL_FEATURES(X) X(graphicsPipelineLibrary)
#define SWAP_MAINT1_FEATURES(X) X(swapchainMaintenance1)
#define PRESENT_WAIT_FEATURES(X) X(presentWait)
#define FEEDBACK_LOOP_FEATURES(X) X(attachmentFeedbackLoopLayout)
#define MODULE_ID_FEATURES(X) X(shaderModuleIdentifier)
#define BORDER_COLOR_FEATURES(X) X(customBorderColors) X(customBorderColorWithoutFormat)
#define MEMORY_PRIORITY_FEATURES(X) X(memoryPriority)

}  // namespace

int main() {
    void* lib = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
    if (!lib) { fprintf(stderr, "dlopen libvulkan.so: %s\n", dlerror()); return 1; }
    auto gipa = reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(lib, "vkGetInstanceProcAddr"));
    if (!gipa) { fprintf(stderr, "vkGetInstanceProcAddr missing\n"); return 1; }
#define GLOBAL(fn) auto fn = reinterpret_cast<PFN_##fn>(gipa(nullptr, #fn))
    GLOBAL(vkCreateInstance);
    GLOBAL(vkEnumerateInstanceVersion);
    uint32_t instanceVersion = VK_API_VERSION_1_0;
    if (vkEnumerateInstanceVersion) vkEnumerateInstanceVersion(&instanceVersion);
    if (instanceVersion < VK_API_VERSION_1_1) { fprintf(stderr, "Vulkan 1.1 loader required\n"); return 1; }

    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "vexa-vkcaps";
    app.apiVersion = instanceVersion < VK_API_VERSION_1_4 ? instanceVersion : VK_API_VERSION_1_4;
    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.pApplicationInfo = &app;
    VkInstance instance = VK_NULL_HANDLE;
    if (VkResult r = vkCreateInstance(&ici, nullptr, &instance); r != VK_SUCCESS) {
        fprintf(stderr, "vkCreateInstance failed %d\n", r);
        return 1;
    }
#define INST(fn) auto fn = reinterpret_cast<PFN_##fn>(gipa(instance, #fn))
    INST(vkEnumeratePhysicalDevices);
    INST(vkEnumerateDeviceExtensionProperties);
    INST(vkGetPhysicalDeviceProperties2);
    INST(vkGetPhysicalDeviceFeatures2);
    INST(vkGetPhysicalDeviceMemoryProperties);
    INST(vkGetPhysicalDeviceFormatProperties);
    INST(vkDestroyInstance);

    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance, &count, nullptr);
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance, &count, devices.data());

    printf("{\"schema\":\"vexa-vkcaps-1\",\"instanceVersion\":\"%s\",\"devices\":[", Version(instanceVersion).c_str());
    for (uint32_t d = 0; d < count; ++d) {
        VkPhysicalDevice pd = devices[d];
        uint32_t extCount = 0;
        vkEnumerateDeviceExtensionProperties(pd, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> exts(extCount);
        vkEnumerateDeviceExtensionProperties(pd, nullptr, &extCount, exts.data());
        std::set<std::string> extNames;
        for (const auto& e : exts) extNames.insert(e.extensionName);
        auto has = [&](const char* name) { return extNames.count(name) != 0; };

        VkPhysicalDeviceDriverProperties driver{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
        VkPhysicalDeviceGraphicsPipelineLibraryPropertiesEXT gplProps{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_PROPERTIES_EXT};
        VkPhysicalDeviceProperties2 props{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
        // Probe the core version first: chained structs must be valid for this device.
        vkGetPhysicalDeviceProperties2(pd, &props);
        const uint32_t api = props.properties.apiVersion;
        const bool hasDriverProps = api >= VK_API_VERSION_1_2 || has(VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME);
        void** tail = &props.pNext;
        if (hasDriverProps) { *tail = &driver; tail = &driver.pNext; }
        if (has(VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME)) { *tail = &gplProps; tail = &gplProps.pNext; }
        vkGetPhysicalDeviceProperties2(pd, &props);

        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        VkPhysicalDeviceVulkan11Features v11{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
        VkPhysicalDeviceVulkan12Features v12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        VkPhysicalDeviceVulkan13Features v13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
        VkPhysicalDeviceVulkan14Features v14{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES};
        VkPhysicalDeviceRobustness2FeaturesKHR robustness2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_KHR};
        VkPhysicalDeviceTransformFeedbackFeaturesEXT xfb{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT};
        VkPhysicalDeviceDepthClipEnableFeaturesEXT depthClip{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_ENABLE_FEATURES_EXT};
        VkPhysicalDeviceMaintenance5Features maint5{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_FEATURES};
        VkPhysicalDeviceMaintenance6Features maint6{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_6_FEATURES};
        VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT gpl{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT};
        VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR swapMaint1{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_KHR};
        VkPhysicalDevicePresentWaitFeaturesKHR presentWait{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR};
        VkPhysicalDeviceAttachmentFeedbackLoopLayoutFeaturesEXT feedbackLoop{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ATTACHMENT_FEEDBACK_LOOP_LAYOUT_FEATURES_EXT};
        VkPhysicalDeviceShaderModuleIdentifierFeaturesEXT moduleId{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_MODULE_IDENTIFIER_FEATURES_EXT};
        VkPhysicalDeviceCustomBorderColorFeaturesEXT borderColor{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT};
        VkPhysicalDeviceMemoryPriorityFeaturesEXT memoryPriority{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT};

        struct ExtFeature { const char* name; const char* alt; void* s; void** next; };
        const ExtFeature extFeatures[] = {
            {VK_KHR_ROBUSTNESS_2_EXTENSION_NAME, VK_EXT_ROBUSTNESS_2_EXTENSION_NAME, &robustness2, &robustness2.pNext},
            {VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME, nullptr, &xfb, &xfb.pNext},
            {VK_EXT_DEPTH_CLIP_ENABLE_EXTENSION_NAME, nullptr, &depthClip, &depthClip.pNext},
            {VK_KHR_MAINTENANCE_5_EXTENSION_NAME, nullptr, &maint5, &maint5.pNext},
            {VK_KHR_MAINTENANCE_6_EXTENSION_NAME, nullptr, &maint6, &maint6.pNext},
            {VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME, nullptr, &gpl, &gpl.pNext},
            {VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME, VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME, &swapMaint1, &swapMaint1.pNext},
            {VK_KHR_PRESENT_WAIT_EXTENSION_NAME, nullptr, &presentWait, &presentWait.pNext},
            {VK_EXT_ATTACHMENT_FEEDBACK_LOOP_LAYOUT_EXTENSION_NAME, nullptr, &feedbackLoop, &feedbackLoop.pNext},
            {VK_EXT_SHADER_MODULE_IDENTIFIER_EXTENSION_NAME, nullptr, &moduleId, &moduleId.pNext},
            {VK_EXT_CUSTOM_BORDER_COLOR_EXTENSION_NAME, nullptr, &borderColor, &borderColor.pNext},
            {VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME, nullptr, &memoryPriority, &memoryPriority.pNext},
        };
        std::set<const void*> queried;
        tail = &features.pNext;
        auto link = [&](void* s, void** next) { *tail = s; tail = next; queried.insert(s); };
        if (api >= VK_API_VERSION_1_2) { link(&v11, &v11.pNext); link(&v12, &v12.pNext); }
        if (api >= VK_API_VERSION_1_3 && app.apiVersion >= VK_API_VERSION_1_3) link(&v13, &v13.pNext);
        if (api >= VK_API_VERSION_1_4 && app.apiVersion >= VK_API_VERSION_1_4) link(&v14, &v14.pNext);
        for (const auto& f : extFeatures) {
            if (has(f.name) || (f.alt && has(f.alt))) link(f.s, f.next);
        }
        vkGetPhysicalDeviceFeatures2(pd, &features);

        printf("%s{", d ? "," : "");
        Obj dev;
        const auto& p = props.properties;
        dev.Str("name", p.deviceName);
        dev.Hex("vendorID", p.vendorID);
        dev.Hex("deviceID", p.deviceID);
        dev.Num("deviceType", p.deviceType);
        dev.Str("apiVersion", Version(api));
        dev.Hex("driverVersion", p.driverVersion);
        if (hasDriverProps) {
            dev.Num("driverID", driver.driverID);
            dev.Str("driverName", driver.driverName);
            dev.Str("driverInfo", driver.driverInfo);
            dev.Str("conformanceVersion", std::to_string(driver.conformanceVersion.major) + "." +
                    std::to_string(driver.conformanceVersion.minor) + "." +
                    std::to_string(driver.conformanceVersion.subminor) + "." +
                    std::to_string(driver.conformanceVersion.patch));
        }

        dev.Open("limits");
        {
            Obj o;
#define LIMIT(n) o.Num(#n, double(p.limits.n));
            LIMITS(LIMIT)
        }
        dev.Close();

        dev.Open("extensions");
        {
            Obj o;
            for (const auto& e : exts) o.Num(e.extensionName, e.specVersion);
        }
        dev.Close();

        dev.Open("features");
        {
            Obj groups;
#define FEATURE(n) o.Bool(#n, s.n);
// Groups that were not chained are omitted: absent means "not queried", not "false".
#define GROUP(key, value, LIST) \
            if (queried.count(&value) || static_cast<const void*>(&value) == &features.features) { \
                groups.Open(key); { Obj o; const auto& s = value; LIST(FEATURE) } groups.Close(); }
            GROUP("core", features.features, CORE_FEATURES)
            GROUP("vulkan11", v11, VK11_FEATURES)
            GROUP("vulkan12", v12, VK12_FEATURES)
            GROUP("vulkan13", v13, VK13_FEATURES)
            GROUP("vulkan14", v14, VK14_FEATURES)
            GROUP("robustness2", robustness2, ROBUSTNESS2_FEATURES)
            GROUP("transformFeedback", xfb, XFB_FEATURES)
            GROUP("depthClipEnable", depthClip, DEPTH_CLIP_FEATURES)
            GROUP("maintenance5", maint5, MAINT5_FEATURES)
            GROUP("maintenance6", maint6, MAINT6_FEATURES)
            GROUP("graphicsPipelineLibrary", gpl, GPL_FEATURES)
            GROUP("swapchainMaintenance1", swapMaint1, SWAP_MAINT1_FEATURES)
            GROUP("presentWait", presentWait, PRESENT_WAIT_FEATURES)
            GROUP("attachmentFeedbackLoopLayout", feedbackLoop, FEEDBACK_LOOP_FEATURES)
            GROUP("shaderModuleIdentifier", moduleId, MODULE_ID_FEATURES)
            GROUP("customBorderColor", borderColor, BORDER_COLOR_FEATURES)
            GROUP("memoryPriority", memoryPriority, MEMORY_PRIORITY_FEATURES)
#undef GROUP
        }
        dev.Close();

        if (has(VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME)) {
            dev.Open("graphicsPipelineLibraryProperties");
            {
                Obj o;
                o.Bool("graphicsPipelineLibraryFastLinking", gplProps.graphicsPipelineLibraryFastLinking);
                o.Bool("graphicsPipelineLibraryIndependentInterpolationDecoration",
                       gplProps.graphicsPipelineLibraryIndependentInterpolationDecoration);
            }
            dev.Close();
        }

        VkPhysicalDeviceMemoryProperties memory{};
        vkGetPhysicalDeviceMemoryProperties(pd, &memory);
        dev.Key("memoryHeaps");
        printf("[");
        for (uint32_t h = 0; h < memory.memoryHeapCount; ++h) {
            printf("%s{\"size\":%llu,\"flags\":\"0x%x\"}", h ? "," : "",
                   (unsigned long long)memory.memoryHeaps[h].size, memory.memoryHeaps[h].flags);
        }
        printf("]");

        dev.Open("formats");
        {
            Obj o;
#define FORMAT(n) { \
                VkFormatProperties fp{}; \
                vkGetPhysicalDeviceFormatProperties(pd, VK_FORMAT_##n, &fp); \
                o.Open("VK_FORMAT_" #n); \
                { Obj f; f.Hex("optimal", fp.optimalTilingFeatures); f.Hex("linear", fp.linearTilingFeatures); \
                  f.Hex("buffer", fp.bufferFeatures); } \
                o.Close(); }
            FORMATS(FORMAT)
#undef FORMAT
        }
        dev.Close();
        printf("}");
    }
    printf("]}\n");
    vkDestroyInstance(instance, nullptr);
    return 0;
}
