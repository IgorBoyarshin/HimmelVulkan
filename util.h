#ifndef UTIL
#define UTIL

#include <vulkan.hpp>


VkImageView createImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; // 1D, 2D, 3D or CubeMap
    viewInfo.format = format;
    // e.g. can map all of the channels to the red channel for a monochrome texture.
    // VK_COMPONENT_SWIZZLE_IDENTITY
    // VK_COMPONENT_SWIZZLE_ZERO
    // VK_COMPONENT_SWIZZLE_ONE
    // VK_COMPONENT_SWIZZLE_R
    // VK_COMPONENT_SWIZZLE_G
    // VK_COMPONENT_SWIZZLE_B
    // VK_COMPONENT_SWIZZLE_A
    viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY; // (defined as 0)
    viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY; // (defined as 0)
    viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY; // (defined as 0)
    viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY; // (defined as 0)
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        std::cerr << "::> Failed to create a VkImageView.\n";
        return VK_NULL_HANDLE;
    }

    return imageView;
}

#endif
