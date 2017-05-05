#pragma once
#include <vulkan/vulkan.h>

VkResult createDebugReportCallbackEXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT*, const VkAllocationCallbacks*, VkDebugReportCallbackEXT*);
void destroyDebugReportCallbackEXT(VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks* pAllocator);