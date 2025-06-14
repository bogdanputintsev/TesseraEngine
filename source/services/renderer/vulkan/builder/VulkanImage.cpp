#include "VulkanImage.h"
#include "engine/EngineCore.h"

#include <stdexcept>

#include "services/renderer/vulkan/utils/VulkanUtils.h"


namespace parus::vulkan
{
	
    VulkanImage::Builder::Builder(std::string debugName)
        : debugName(std::move(debugName))
    {
    }

    VulkanImage VulkanImage::Builder::build(const VulkanStorage& storage) const
    {
    	VulkanImage result{};

        VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = width;
		imageInfo.extent.height = height;
		imageInfo.extent.depth = depth;
		imageInfo.mipLevels = mipLevels;
		imageInfo.arrayLayers = arrayLayers;
		imageInfo.format = format;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.flags = flags;

    	ASSERT(vkCreateImage(storage.device, &imageInfo, nullptr, &result.image) == VK_SUCCESS,
    		"Failed to create a new image");

    	utils::setDebugObjectName(storage, result.image, VK_OBJECT_TYPE_IMAGE, debugName + " Image");
    	
		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(storage.device, result.image, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = utils::findMemoryType(storage, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		ASSERT(vkAllocateMemory(storage.device, &allocInfo, nullptr, &result.deviceMemory) == VK_SUCCESS,
			"Failed to allocate a new image memory");
    	
    	utils::setDebugObjectName(storage, result.deviceMemory, VK_OBJECT_TYPE_DEVICE_MEMORY, debugName + " Device Memory");

		vkBindImageMemory(storage.device, result.image, result.deviceMemory, 0);

		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = result.image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		viewInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
		viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
		viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
		viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 6;

		ASSERT(vkCreateImageView(storage.device, &viewInfo, nullptr, &result.imageView) == VK_SUCCESS,
			"Failed to create a new image view!");

    	utils::setDebugObjectName(storage, result.imageView, VK_OBJECT_TYPE_IMAGE_VIEW, debugName + " Image View");

		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = 1.0f;

		ASSERT(vkCreateSampler(storage.device, &samplerInfo, nullptr, &result.sampler) == VK_SUCCESS,
			"Failed to create a new image sampler.");
    	
    	utils::setDebugObjectName(storage, result.sampler, VK_OBJECT_TYPE_SAMPLER, debugName + " Sampler");

	    return result;
    }

    VulkanImage::Builder& VulkanImage::Builder::setExtent(
    	const uint32_t newWidth,
    	const uint32_t newHeight,
	    const uint32_t newDepth)
    {
    	width = newWidth;
    	height = newHeight;
    	depth = newDepth;

    	return *this;
    }

    VulkanImage::Builder& VulkanImage::Builder::setMipLevels(const uint32_t newMipLevels)
    {
    	mipLevels = newMipLevels;
    	return *this;
    }

    VulkanImage::Builder& VulkanImage::Builder::setArrayLayers(const uint32_t newArrayLayers)
    {
    	arrayLayers = newArrayLayers;
    	return *this;
    }

    VulkanImage::Builder& VulkanImage::Builder::setFormat(const VkFormat newFormat)
    {
    	format = newFormat;
    	return *this;
    }

    VulkanImage::Builder& VulkanImage::Builder::setFlags(const VkImageCreateFlags newFlags)
    {
    	flags = newFlags;
    	return *this;
    }
}
