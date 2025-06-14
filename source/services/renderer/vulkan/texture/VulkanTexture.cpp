#include "VulkanTexture.h"

#define STB_IMAGE_IMPLEMENTATION
#include <third-party/stb_image.h>

#include "engine/EngineCore.h"
#include "services/renderer/vulkan/VulkanRenderer.h"
#include "services/Services.h"

namespace parus::vulkan
{
	VulkanTexture importTextureFromFile(const std::string& filePath)
	{
	    ASSERT(std::filesystem::exists(filePath),
				"File " + filePath + " must exist.");
    	ASSERT(std::filesystem::is_regular_file(filePath),
			"File " + filePath + " must be character file."); 
    	
		VulkanTexture newTexture{};
		LOG_INFO("Importing texture: " + filePath);

		int textureWidth;
		int textureHeight;
		int textureChannels;
		stbi_uc* pixels = stbi_load(filePath.c_str(), &textureWidth, &textureHeight, &textureChannels, STBI_rgb_alpha);
		
		const VkDeviceSize imageSize = static_cast<uint64_t>(textureWidth) * static_cast<uint64_t>(textureHeight) * 4;
		newTexture.maxMipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(textureWidth, textureHeight)))) + 1;

		ASSERT(pixels, "failed to load texture image.");

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;

    	const auto& vulkanRenderer = Services::get<VulkanRenderer>();
		vulkanRenderer->createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		void* data;
		vkMapMemory(vulkanRenderer->logicalDevice, stagingBufferMemory, 0, imageSize, 0, &data);
		memcpy(data, pixels, static_cast<size_t>(imageSize));
		vkUnmapMemory(vulkanRenderer->logicalDevice, stagingBufferMemory);
		
		stbi_image_free(pixels);

		vulkanRenderer->createImage(textureWidth, textureHeight,
			newTexture.maxMipLevels,
			VK_SAMPLE_COUNT_1_BIT,
			VK_FORMAT_R8G8B8A8_SRGB,
			VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			newTexture.image,
			newTexture.imageMemory);
		
		vulkanRenderer->transitionImageLayout(newTexture.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, newTexture.maxMipLevels);
		vulkanRenderer->copyBufferToImage(stagingBuffer, newTexture.image, static_cast<uint32_t>(textureWidth), static_cast<uint32_t>(textureHeight));

		vkDestroyBuffer(vulkanRenderer->logicalDevice, stagingBuffer, nullptr);
		vkFreeMemory(vulkanRenderer->logicalDevice, stagingBufferMemory, nullptr);

		vulkanRenderer->generateMipmaps(newTexture, VK_FORMAT_R8G8B8A8_SRGB, textureWidth, textureHeight);

		newTexture.imageView = vulkanRenderer->createImageView(newTexture.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, newTexture.maxMipLevels);
		newTexture.sampler = vulkanRenderer->createTextureSampler(newTexture.maxMipLevels);

        return newTexture;
	}

	VulkanTexture createSolidColorTexture(const math::Vector3& color)
	{
		const auto& vulkanRenderer = Services::get<vulkan::VulkanRenderer>();

		// 1. Create VkImage
		VkImage image;
		
		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		imageInfo.extent = { 1, 1, 1 };
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		vkCreateImage(vulkanRenderer->logicalDevice, &imageInfo, nullptr, &image);

		// 2. Allocate Memory
		VkDeviceMemory imageMemory;
		
		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(vulkanRenderer->logicalDevice, image, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = vulkanRenderer->findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		vkAllocateMemory(vulkanRenderer->logicalDevice, &allocInfo, nullptr, &imageMemory);
		vkBindImageMemory(vulkanRenderer->logicalDevice, image, imageMemory, 0);
		vulkanRenderer->setDebugObjectName(reinterpret_cast<uint64_t>(imageMemory), VK_OBJECT_TYPE_DEVICE_MEMORY, "DEFAULT_TEXTURE_DEVICE_MEMORY");

		// 3. Transition Image Layout
		vulkanRenderer->transitionImageLayout(
			image,
			VK_FORMAT_R8G8B8A8_UNORM, 
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1);

		// 4. Upload Solid Color
		const uint8_t colorData[4] = {
			static_cast<uint8_t>(color.x * 255.0f),
			static_cast<uint8_t>(color.y * 255.0f),
			static_cast<uint8_t>(color.z * 255.0f),
			255
		};
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		vulkanRenderer->createBuffer(sizeof(colorData), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
			stagingBuffer, stagingBufferMemory);

		vulkanRenderer->setDebugObjectName(reinterpret_cast<uint64_t>(stagingBuffer), VK_OBJECT_TYPE_BUFFER, "DEFAULT_TEXTURE_BUFFER");
		vulkanRenderer->setDebugObjectName(reinterpret_cast<uint64_t>(stagingBufferMemory), VK_OBJECT_TYPE_DEVICE_MEMORY, "DEFAULT_TEXTURE_DEVICE_MEMORY");
		
		void* data;
		vkMapMemory(vulkanRenderer->logicalDevice, stagingBufferMemory, 0, sizeof(colorData), 0, &data);
		memcpy(data, colorData, sizeof(colorData));
		vkUnmapMemory(vulkanRenderer->logicalDevice, stagingBufferMemory);

		vulkanRenderer->copyBufferToImage(stagingBuffer, image, 1, 1);

		vkDestroyBuffer(vulkanRenderer->logicalDevice, stagingBuffer, nullptr);
		vkFreeMemory(vulkanRenderer->logicalDevice, stagingBufferMemory, nullptr);

		// 5. Transition to Shader-Readable Layout
		vulkanRenderer->transitionImageLayout(
			image,
			VK_FORMAT_R8G8B8A8_UNORM, 
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			1);

		// 6. Create Image View
		VkImageView imageView;
		
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		vkCreateImageView(vulkanRenderer->logicalDevice, &viewInfo, nullptr, &imageView);

		// 7. Create Sampler
		VkSampler sampler;
		
		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.maxAnisotropy = 1.0f;
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

		vkCreateSampler(vulkanRenderer->logicalDevice, &samplerInfo, nullptr, &sampler);

		// 8. Return Result
		const VulkanTexture newTexture =
			{
				.image = image,
				.imageMemory = imageMemory,
				.imageView = imageView,
				.sampler = sampler,
				.maxMipLevels = 1
			};

		return newTexture;
	}
}
