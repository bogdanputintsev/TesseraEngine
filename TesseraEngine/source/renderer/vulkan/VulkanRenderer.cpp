#include "VulkanRenderer.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

#include "Core.h"
#include "core/platform/Platform.h"
#include "importers/mesh/ObjImporter.h"
#include "math/UniformBufferObjects.h"
#include "texture/Texture.h"
#include "utils/TesseraLog.h"

namespace tessera::vulkan
{
	void VulkanRenderer::init()
	{
		REGISTER_EVENT(EventType::EVENT_WINDOW_RESIZED, [&](const int newWidth, const int newHeight)
		{
			LOG_INFO("Vulkan initiated window resize. New dimensions: " + std::to_string(newWidth) + " " + std::to_string(newHeight));
			onResize();
		});

		REGISTER_EVENT(EventType::EVENT_APPLICATION_QUIT, [&]([[maybe_unused]]const int exitCode)
		{
			isRunning = false;
		});
		
		createInstance();
		createDebugManager();
		createSurface();
		createDevices();
		createQueues();
		createSwapChain();
		createImageViews();
		createRenderPass();
		createDescriptorSetLayout();
		createGraphicsPipeline();
		createColorResources();
		createDepthResources();
		createFramebuffers();

		createUniformBuffer();
		createDescriptorPool();
		
		importMesh("bin/assets/skybox/skybox.obj");
		RUN_ASYNC(importMesh("bin/assets/terrain/floor.obj"););
		RUN_ASYNC(importMesh("bin/assets/indoor/indoor.obj"););
		RUN_ASYNC(importMesh("bin/assets/indoor/threshold.obj"););
		RUN_ASYNC(importMesh("bin/assets/indoor/torch.obj"););

		createCommandBuffer();
		createSyncObjects();

		isRunning = true;

	}

	void VulkanRenderer::clean()
	{
		cleanupSwapChain();

		vkDestroyPipeline(logicalDevice, graphicsPipeline, nullptr);
		vkDestroyPipelineLayout(logicalDevice, pipelineLayout, nullptr);
		vkDestroyRenderPass(logicalDevice, renderPass, nullptr);

		for (size_t i = 0; i < VulkanRenderer::MAX_FRAMES_IN_FLIGHT; i++)
		{
			vkDestroyBuffer(logicalDevice, globalUboBuffers[i], nullptr);
			vkDestroyBuffer(logicalDevice, instanceUboBuffers[i], nullptr);
			vkFreeMemory(logicalDevice, globalUboMemory[i], nullptr);
			vkFreeMemory(logicalDevice, instanceUboMemory[i], nullptr);
		}

		vkDestroyDescriptorPool(logicalDevice, descriptorPool, nullptr);
		
		for (const auto& texture : CORE->world.getStorage()->getTextures())
		{
			vkDestroySampler(logicalDevice, texture->textureSampler, nullptr);
			vkDestroyImageView(logicalDevice, texture->textureImageView, nullptr);
			vkDestroyImage(logicalDevice, texture->textureImage, nullptr);
			vkFreeMemory(logicalDevice, texture->textureImageMemory, nullptr);
		}

		vkDestroyDescriptorSetLayout(logicalDevice, descriptorSetLayout, nullptr);

		vkDestroyBuffer(logicalDevice, globalBuffers.indexBuffer, nullptr);
		vkFreeMemory(logicalDevice, globalBuffers.indexBufferMemory, nullptr);

		vkDestroyBuffer(logicalDevice, globalBuffers.vertexBuffer, nullptr);
		vkFreeMemory(logicalDevice, globalBuffers.vertexBufferMemory, nullptr);

		for (size_t i = 0; i < VulkanRenderer::MAX_FRAMES_IN_FLIGHT; i++)
		{
			vkDestroySemaphore(logicalDevice, renderFinishedSemaphores[i], nullptr);
			vkDestroySemaphore(logicalDevice, imageAvailableSemaphores[i], nullptr);
			vkDestroyFence(logicalDevice, inFlightFences[i], nullptr);
		}

		for (const auto& [_, commandPool]: threadCommandPools)
		{
			vkDestroyCommandPool(logicalDevice, commandPool, nullptr);
		}

		vkDestroyDevice(logicalDevice, nullptr);

		if (VulkanRenderer::validationLayersAreEnabled())
		{
			destroyDebugUtilsMessengerExt(debugMessenger, nullptr);
		}

		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);
	}

	void VulkanRenderer::drawFrame()
	{
		if (!isRunning)
		{
			return;
		}
		
		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

		cleanupFrameResources();
		
		vkWaitForFences(logicalDevice, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

		const std::optional<uint32_t> imageIndex = acquireNextImage();
		if (!imageIndex.has_value())
		{
			return;
		}

		updateUniformBuffer(currentFrame);
		processLoadedMeshes();
		vkResetFences(logicalDevice, 1, &inFlightFences[currentFrame]);

		const auto commandBuffer = getCommandBuffer(currentFrame);

		resetCommandBuffer(currentFrame);
		recordCommandBuffer(commandBuffer, imageIndex.value());

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		const VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
		constexpr VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

		const VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		ASSERT(threadSafeQueueSubmit(&submitInfo, inFlightFences[currentFrame]) == VK_SUCCESS, "failed to submit draw command buffer.");

		// Submit result back to swap chain to have it eventually show up on the screen. 
		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		const VkSwapchainKHR swapChains[] = { swapChain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex.value();
		presentInfo.pResults = nullptr;

		
		const VkResult result = threadSafePresent(&presentInfo);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized)
		{
			framebufferResized = false;
			recreateSwapChain();
		}
		else
		{
			ASSERT(result == VK_SUCCESS, "failed to present swap chain image.");
		}

	}

	void VulkanRenderer::deviceWaitIdle()
	{
		vkDeviceWaitIdle(logicalDevice);
	}
	
	void VulkanRenderer::importMesh(const std::string& meshPath)
	{
		Mesh newMesh = ObjImporter::importFromFile(meshPath);
		std::lock_guard lock(importModelMutex);
		modelQueue.emplace(meshPath, std::make_shared<Mesh>(newMesh));
	}

	void VulkanRenderer::processLoadedMeshes()
	{
		std::unique_lock lock(importModelMutex);
		if (modelQueue.empty())
		{
			return;	
		}
		
		while (!modelQueue.empty())
		{
			auto [meshPath, newMesh] = modelQueue.front();
			modelQueue.pop();
			lock.unlock();
			
			CORE->world.getStorage()->addNewMesh(meshPath, newMesh);

			models.push_back({
				.mesh = newMesh,
				.transform = math::Matrix4x4::identity()
			});
		}
		
		std::vector<math::Vertex> allVertices;
		std::vector<uint32_t> allIndices;

		for (const auto& mesh : CORE->world.getStorage()->getMeshes())
		{
			for (auto& meshPart : mesh->meshParts)
			{
				meshPart.vertexOffset = allVertices.size();
				meshPart.indexOffset = allIndices.size();

				allVertices.insert(allVertices.end(), meshPart.vertices.begin(), meshPart.vertices.end());
				allIndices.insert(allIndices.end(), meshPart.indices.begin(), meshPart.indices.end());
			}
		}
		
		createVertexBuffer(allVertices);
		createIndexBuffer(allIndices);
		
		globalBuffers.totalVertices = allVertices.size();
		globalBuffers.totalIndices = allIndices.size();

		for (auto& mesh : CORE->world.getStorage()->getMeshes())
		{
			createDescriptorSets(mesh);
		}
	}

	void VulkanRenderer::createInstance()
	{
		checkValidationLayerSupport();

		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Sandbox";
		appInfo.applicationVersion = VK_MAKE_API_VERSION(1, 0, 0, 0);
		appInfo.pEngineName = "Tessera Engine";
		appInfo.engineVersion = VK_MAKE_API_VERSION(1, 0, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		checkIfAllRequiredExtensionsAreSupported();

		const std::vector<const char*> requiredExtensions = getRequiredExtensions();
		createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
		createInfo.ppEnabledExtensionNames = requiredExtensions.data();

		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};

		const std::vector<const char*> validationLayers = getValidationLayers();
		if (validationLayersAreEnabled())
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();

			populate(debugCreateInfo);
			createInfo.pNext = &debugCreateInfo;
		}
		else
		{
			createInfo.enabledLayerCount = 0;
			populate(debugCreateInfo);

			createInfo.pNext = nullptr;
		}

		ASSERT(vkCreateInstance(&createInfo, nullptr, &instance) == VK_SUCCESS, "failed to create instance");
	}

	void VulkanRenderer::checkValidationLayerSupport()
	{
		if (!validationLayersAreEnabled())
		{
			return;
		}

		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		bool result = true;

		for (const char* layerName : getValidationLayers())
		{
			bool layerFound = false;

			for (const auto& layerProperties : availableLayers)
			{
				if (strcmp(layerName, layerProperties.layerName) == 0)
				{
					layerFound = true;
					break;
				}
			}

			if (!layerFound)
			{
				result = false;
				break;
			}
		}

		ASSERT(result, "validation layers requested, but not available.");

	}

	bool VulkanRenderer::validationLayersAreEnabled()
	{
#ifdef IN_DEBUG_MODE
		return true;
#else
		return false;
#endif
	}

	std::vector<const char*> VulkanRenderer::getValidationLayers()
	{
		return { "VK_LAYER_KHRONOS_validation" };
	}

	void VulkanRenderer::checkIfAllRequiredExtensionsAreSupported()
	{
		const std::vector<const char*> requiredExtensions = getRequiredExtensions();
		const std::unordered_set<std::string> requiredExtensionsSet{ requiredExtensions.begin(), requiredExtensions.end() };

		uint32_t matches = 0;

		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
		std::vector<VkExtensionProperties> extensions(extensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

		LOG_DEBUG("List of all available extensions:");

		for (const auto& [extensionName, specVersion] : extensions)
		{
			LOG_DEBUG("\t" + std::string(extensionName));

			if (requiredExtensionsSet.contains(std::string(extensionName)))
			{
				++matches;
			}
		}

		ASSERT(matches == requiredExtensions.size(), "All required Vulkan extensions muse be supported.");
	}

	void VulkanRenderer::destroyDebugUtilsMessengerExt(VkDebugUtilsMessengerEXT debugMessengerToDestroy, const VkAllocationCallbacks* pAllocator) const
	{
		const auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
		if (func != nullptr)
		{
			func(instance, debugMessengerToDestroy, pAllocator);
		}
	}

	std::vector<const char*> VulkanRenderer::getRequiredExtensions()
	{
		std::vector extensions = CORE->graphicsLibrary->getRequiredExtensions();

		if (validationLayersAreEnabled())
		{
			extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		return extensions;
	}

	VkBool32 VulkanRenderer::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		[[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		[[maybe_unused]] void* pUserData)
	{
		LOG(getLogType(messageSeverity), pCallbackData->pMessage);
		return VK_FALSE;
	}

	void VulkanRenderer::populate(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
	{
		createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = debugCallback;
	}

	LogType VulkanRenderer::getLogType(const VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity)
	{
		switch (messageSeverity)
		{
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
			return LogType::DEBUG;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
			return LogType::INFO;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
			return LogType::WARNING;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
			return LogType::TE_ERROR;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_FLAG_BITS_MAX_ENUM_EXT:
			return LogType::INFO;
		}

		return LogType::INFO;
	}

	void VulkanRenderer::createDebugManager()
	{
		if (!validationLayersAreEnabled())
		{
			return;
		}

		VkDebugUtilsMessengerCreateInfoEXT createInfo{};
		populate(createInfo);

		ASSERT(createDebugUtilsMessengerExt(&createInfo, nullptr, &debugMessenger) == VK_SUCCESS, "failed to set up debug messenger.");
	}

	VkResult VulkanRenderer::createDebugUtilsMessengerExt(const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
		const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) const
	{
		const auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
		if (func != nullptr)
		{
			return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
		}

		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}

	void VulkanRenderer::createSurface()
	{
		//const std::shared_ptr<GLFWwindow>& window = CORE->graphicsLibrary->getWindow();
		//ASSERT(glfwCreateWindowSurface(instance, window.get(), nullptr, &surface) == VK_SUCCESS, "failed to create window surface.");
		surface = CORE->platform->createVulkanSurface(instance);
	}

	QueueFamilyIndices findQueueFamilies(const VkPhysicalDevice& physicalDevice, const VkSurfaceKHR& surface)
	{
		QueueFamilyIndices familyIndices;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

		for (uint32_t i = 0; i < queueFamilyCount; ++i)
		{
			if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				familyIndices.graphicsFamily = i;
			}

			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
			if (presentSupport)
			{
				familyIndices.presentFamily = i;
			}

			if (familyIndices.isComplete())
			{
				break;
			}
		}

		return familyIndices;
	}

	// Logical device extensions.
	std::vector<const char*> getRequiredDeviceExtensions()
	{
		return { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	}

	bool isDeviceExtensionSupported(const VkPhysicalDevice& device)
	{
		const std::vector<const char*> requiredExtensions = getRequiredDeviceExtensions();

		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		// TODO: Optimize with O(1) complexity.
		std::set<std::string> requiredExtensionsSet(requiredExtensions.begin(), requiredExtensions.end());

		for (const auto& extension : availableExtensions)
		{
			requiredExtensionsSet.erase(extension.extensionName);
		}

		return requiredExtensionsSet.empty();
	}

	void VulkanRenderer::createDevices()
	{
		pickAnySuitableDevice();

		// Create logical device.
		ASSERT(physicalDevice, "Devices hasn't been picked successfully.");

		const auto [graphicsFamily, presentFamily] = findQueueFamilies(physicalDevice, surface);
		ASSERT(graphicsFamily.has_value() && presentFamily.has_value(), "queue family indices are not complete.");

		std::set uniqueQueueFamilies = { graphicsFamily.value(), presentFamily.value() };

		constexpr float queuePriority = 1.0f;
		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

		for (uint32_t queueFamily : uniqueQueueFamilies)
		{
			VkDeviceQueueCreateInfo queueCreateInfo{};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.emplace_back(queueCreateInfo);
		}

		// Specifying used device features.
		VkPhysicalDeviceFeatures deviceFeatures{};
		deviceFeatures.samplerAnisotropy = VK_TRUE;

		VkDeviceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.pEnabledFeatures = &deviceFeatures;

		const auto requiredExtensions = getRequiredDeviceExtensions();
		createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
		createInfo.ppEnabledExtensionNames = requiredExtensions.data();

		// Distinction between instance and device specific validation no longer the case. This was added for back compatibility.
		const std::vector<const char*> validationLayers = getValidationLayers();
		if (validationLayersAreEnabled())
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else
		{
			createInfo.enabledLayerCount = 0;
		}

		ASSERT(vkCreateDevice(physicalDevice, &createInfo, nullptr, &logicalDevice) == VK_SUCCESS, "failed to create logical device.");
	}

	void VulkanRenderer::pickAnySuitableDevice()
	{
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

		ASSERT(deviceCount != 0, "failed to find GPUs with Vulkan support.");

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

		for (const auto& device : devices)
		{
			if (isDeviceSuitable(device, surface))
			{
				physicalDevice = device;
				msaaSamples = getMaxUsableSampleCount();
				break;
			}
		}

		ASSERT(physicalDevice != VK_NULL_HANDLE, "failed to find a suitable GPU.");
	}

	bool VulkanRenderer::isDeviceSuitable(const VkPhysicalDevice& device, const VkSurfaceKHR& surface)
	{
		// Basic device properties like the name, type and supported Vulkan version.
		[[maybe_unused]] VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(device, &deviceProperties);

		// The support for optional features like texture compression, 64 bit floats and multi viewport rendering.
		[[maybe_unused]] VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

		// Check if device can process the commands we want to use.
		const QueueFamilyIndices queueFamilyIndices = findQueueFamilies(device, surface);

		// Check if physical device supports swap chain extension.
		const bool extensionsSupported = isDeviceExtensionSupported(device);

		// Check if physical device supports swap chain.
		const SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device, surface);

		// Check anisotropic filtering
		VkPhysicalDeviceFeatures supportedFeatures;
		vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

		return queueFamilyIndices.isComplete()
			&& extensionsSupported
			&& swapChainSupport.isComplete()
			&& supportedFeatures.samplerAnisotropy;
	}

	SwapChainSupportDetails VulkanRenderer::querySwapChainSupport(const VkPhysicalDevice& device, const VkSurfaceKHR& surface)
	{
		SwapChainSupportDetails details;

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

		if (formatCount != 0)
		{
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
		}

		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

		if (presentModeCount != 0)
		{
			details.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
		}

		return details;
	}

	void VulkanRenderer::createQueues()
	{
		const auto [graphicsFamily, presentFamily] = findQueueFamilies(physicalDevice, surface);

		ASSERT(graphicsFamily.has_value() && presentFamily.has_value(), "queue family is undefined.");

		std::lock_guard lock(graphicsQueueMutex);
		vkGetDeviceQueue(logicalDevice, graphicsFamily.value(), 0, &graphicsQueue);
		vkGetDeviceQueue(logicalDevice, presentFamily.value(), 0, &presentQueue);
	}

	void VulkanRenderer::createSwapChain()
	{
		const auto [capabilities, formats, presentModes] = querySwapChainSupport(physicalDevice, surface);

		const auto [format, colorSpace] = chooseSwapSurfaceFormat(formats);
		const VkPresentModeKHR presentMode = chooseSwapPresentMode(presentModes);
		const VkExtent2D extent = chooseSwapExtent(capabilities);

		uint32_t imageCount = capabilities.minImageCount + 1;
		if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
		{
			imageCount = capabilities.maxImageCount;
		}

		ASSERT(extent.width != 0 && extent.height != 0, "Swap chain extent is invalid (window may be minimized)");

		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = surface;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = format;
		createInfo.imageColorSpace = colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		const auto [graphicsFamily, presentFamily] = findQueueFamilies(physicalDevice, surface);
		ASSERT(graphicsFamily.has_value() && presentFamily.has_value(), "Queue families are not complete.");
		const uint32_t queueFamilyIndices[] = { graphicsFamily.value(), presentFamily.value() };

		if (graphicsFamily != presentFamily) {
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else {
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0;
			createInfo.pQueueFamilyIndices = nullptr;
		}

		createInfo.preTransform = capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = VK_NULL_HANDLE;

		ASSERT(vkCreateSwapchainKHR(logicalDevice, &createInfo, nullptr, &swapChain) == VK_SUCCESS, "failed to create swap chain.");

		std::vector<VkImage> swapChainImages;
		vkGetSwapchainImagesKHR(logicalDevice, swapChain, &imageCount, nullptr);
		swapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(logicalDevice, swapChain, &imageCount, swapChainImages.data());

		swapChainDetails = { format, extent, swapChainImages };
	}

	VkSurfaceFormatKHR VulkanRenderer::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
	{
		assert(!availableFormats.empty());

		for (const auto& availableFormat : availableFormats)
		{
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				return availableFormat;
			}
		}

		return availableFormats[0];
	}

	VkPresentModeKHR VulkanRenderer::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
	{
		// TODO: Replace with std::find.
		for (const auto& availablePresentMode : availablePresentModes)
		{
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				return availablePresentMode;
			}
		}

		// Only the VK_PRESENT_MODE_FIFO_KHR mode is guaranteed to be available.
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D VulkanRenderer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
	{
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		{
			return capabilities.currentExtent;
		}

		const auto windowInfo = CORE->platform->getWindowInfo();
		const int width = windowInfo.width;
		const int height = windowInfo.height;

		VkExtent2D actualExtent = {
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height)
		};

		actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

		return actualExtent;
	}

	std::optional<uint32_t> VulkanRenderer::acquireNextImage()
	{
		ASSERT(static_cast<size_t>(currentFrame) < imageAvailableSemaphores.size() && currentFrame >= 0, "current frame number is larger than number of fences.");

		uint32_t imageIndex;
		const VkResult result = vkAcquireNextImageKHR(logicalDevice, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			recreateSwapChain();
			return std::nullopt;
		}

		ASSERT(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR, "failed to acquire swap chain image.");

		return imageIndex;
	}

	void VulkanRenderer::recreateSwapChain()
	{
		CORE->graphicsLibrary->handleMinimization();
		CORE->renderer->deviceWaitIdle();

		cleanupSwapChain();

		createSwapChain();
		createImageViews();
		createColorResources();
		createDepthResources();
		createFramebuffers();
	}

	void VulkanRenderer::cleanupSwapChain() const
	{
		vkDestroyImageView(logicalDevice, depthImageView, nullptr);
		vkDestroyImage(logicalDevice, depthImage, nullptr);
		vkFreeMemory(logicalDevice, depthImageMemory, nullptr);

		vkDestroyImageView(logicalDevice, colorImageView, nullptr);
		vkDestroyImage(logicalDevice, colorImage, nullptr);
		vkFreeMemory(logicalDevice, colorImageMemory, nullptr);

		for (const auto framebuffer : swapChainFramebuffers) {
			vkDestroyFramebuffer(logicalDevice, framebuffer, nullptr);
		}

		for (const auto imageView : swapChainImageViews) {
			vkDestroyImageView(logicalDevice, imageView, nullptr);
		}

		vkDestroySwapchainKHR(logicalDevice, swapChain, nullptr);
	}

	void VulkanRenderer::createImageViews()
	{
		const auto& [swapChainImageFormat, swapChainExtent, swapChainImages]
			= swapChainDetails;

		swapChainImageViews.resize(swapChainImages.size());

		for (size_t i = 0; i < swapChainImageViews.size(); i++)
		{
			swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
		}
	}

	VkImageView VulkanRenderer::createImageView(const VkImage image, const VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels) const
	{
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = format;
		viewInfo.subresourceRange.aspectMask = aspectFlags;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = mipLevels;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		VkImageView imageView;
		ASSERT(vkCreateImageView(logicalDevice, &viewInfo, nullptr, &imageView) == VK_SUCCESS, "failed to create texture image view.");

		return imageView;
	}

	void VulkanRenderer::createRenderPass()
	{
		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = swapChainDetails.swapChainImageFormat;
		colorAttachment.samples = msaaSamples;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription depthAttachment{};
		depthAttachment.format = findDepthFormat();
		depthAttachment.samples = msaaSamples;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription colorAttachmentResolve{};
		colorAttachmentResolve.format = swapChainDetails.swapChainImageFormat;
		colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorAttachmentRef{};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthAttachmentRef{};
		depthAttachmentRef.attachment = 1;
		depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference colorAttachmentResolveRef{};
		colorAttachmentResolveRef.attachment = 2;
		colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;
		subpass.pResolveAttachments = &colorAttachmentResolveRef;
		subpass.pDepthStencilAttachment = &depthAttachmentRef;

		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		const std::array attachments = { colorAttachment, depthAttachment, colorAttachmentResolve };

		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		renderPassInfo.pAttachments = attachments.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		ASSERT(vkCreateRenderPass(logicalDevice, &renderPassInfo, nullptr, &renderPass) == VK_SUCCESS, "failed to create render pass.");
	}

	void VulkanRenderer::createDescriptorSetLayout()
	{
		// Binding 0: Global UBO
		VkDescriptorSetLayoutBinding globalUboBinding{};
		globalUboBinding.binding = 0;
		globalUboBinding.descriptorCount = 1;
		globalUboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		globalUboBinding.pImmutableSamplers = nullptr;
		globalUboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		// Binding 1: Instance UBO
		VkDescriptorSetLayoutBinding instanceUboBinding{};
		instanceUboBinding.binding = 1;
		instanceUboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		instanceUboBinding.descriptorCount = 1;
		instanceUboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		instanceUboBinding.pImmutableSamplers = nullptr;

		// Binding 2: Combined image sampler (for the texture)
		VkDescriptorSetLayoutBinding samplerLayoutBinding{};
		samplerLayoutBinding.binding = 2;
		samplerLayoutBinding.descriptorCount = 1;
		samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerLayoutBinding.pImmutableSamplers = nullptr;
		samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		const std::array bindings = { globalUboBinding, instanceUboBinding, samplerLayoutBinding };
		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
		layoutInfo.pBindings = bindings.data();

		ASSERT(vkCreateDescriptorSetLayout(logicalDevice, &layoutInfo, nullptr, &descriptorSetLayout) == VK_SUCCESS, "failed to create descriptor set layout.");
	}

	void VulkanRenderer::createGraphicsPipeline()
	{
		const auto vertexShaderCode = utils::readFile("bin/shaders/vert.spv");
		VkShaderModule vertexShaderModule = createShaderModule(vertexShaderCode, logicalDevice);

		const auto fragmentShaderCode = utils::readFile("bin/shaders/frag.spv");
		VkShaderModule fragmentShaderModule = createShaderModule(fragmentShaderCode, logicalDevice);

		VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = vertexShaderModule;
		vertShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = fragmentShaderModule;
		fragShaderStageInfo.pName = "main";

		[[maybe_unused]] VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

		// Vertex input
		auto bindingDescription = getBindingDescription();
		auto attributeDescriptions = getAttributeDescriptions();

		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

		// Input assembly
		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.scissorCount = 1;

		// Rasterizer
		VkPipelineRasterizationStateCreateInfo rasterizer{};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;
		rasterizer.rasterizerDiscardEnable = VK_FALSE;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;

		
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizer.depthBiasEnable = VK_FALSE;
		rasterizer.depthBiasConstantFactor = 0.0f;
		rasterizer.depthBiasClamp = 0.0f;
		rasterizer.depthBiasSlopeFactor = 0.0f;

		// Multisampling
		VkPipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.minSampleShading = 1.0f;
		multisampling.rasterizationSamples = msaaSamples;
		multisampling.pSampleMask = nullptr;
		multisampling.alphaToCoverageEnable = VK_FALSE;
		multisampling.alphaToOneEnable = VK_FALSE;

		// Depth stencil
		VkPipelineDepthStencilStateCreateInfo depthStencil{};
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable = VK_TRUE;
		depthStencil.depthWriteEnable = VK_TRUE;
		depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
		depthStencil.depthBoundsTestEnable = VK_FALSE;
		depthStencil.stencilTestEnable = VK_FALSE;

		// Color blending.
		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_FALSE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

		VkPipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = VK_LOGIC_OP_COPY;
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;
		colorBlending.blendConstants[0] = 0.0f;
		colorBlending.blendConstants[1] = 0.0f;
		colorBlending.blendConstants[2] = 0.0f;
		colorBlending.blendConstants[3] = 0.0f;

		// Dynamic state
		std::vector dynamicStates = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};

		VkPipelineDynamicStateCreateInfo dynamicState{};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
		dynamicState.pDynamicStates = dynamicStates.data();

		// Pipeline layout.
		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 1;
		pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
		pipelineLayoutInfo.pushConstantRangeCount = 0;
		pipelineLayoutInfo.pPushConstantRanges = nullptr;

		ASSERT(vkCreatePipelineLayout(logicalDevice, &pipelineLayoutInfo, nullptr, &pipelineLayout) == VK_SUCCESS, "failed to create pipeline layout.");

		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = shaderStages;
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pDepthStencilState = &depthStencil;
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDynamicState = &dynamicState;
		pipelineInfo.layout = pipelineLayout;
		pipelineInfo.renderPass = renderPass;
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineInfo.basePipelineIndex = -1;

		ASSERT(vkCreateGraphicsPipelines(logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) == VK_SUCCESS, "failed to create graphics pipeline.");
		vkDestroyShaderModule(logicalDevice, fragmentShaderModule, nullptr);
		vkDestroyShaderModule(logicalDevice, vertexShaderModule, nullptr);
	}

	VkShaderModule VulkanRenderer::createShaderModule(const std::vector<char>& code, const VkDevice& device)
	{
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

		VkShaderModule shaderModule;
		ASSERT(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) == VK_SUCCESS, "failed to create shader module.");

		return shaderModule;
	}

	void VulkanRenderer::createFramebuffers()
	{
		swapChainFramebuffers.resize(swapChainImageViews.size());

		for (size_t i = 0; i < swapChainImageViews.size(); ++i)
		{
			std::array attachments = {
				colorImageView,
				depthImageView,
				swapChainImageViews[i]
			};

			const auto& [width, height] = swapChainDetails.swapChainExtent;

			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = renderPass;
			framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
			framebufferInfo.pAttachments = attachments.data();
			framebufferInfo.width = width;
			framebufferInfo.height = height;
			framebufferInfo.layers = 1;

			ASSERT(vkCreateFramebuffer(logicalDevice, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) == VK_SUCCESS, "failed to create framebuffer.");
		}
	}

	void VulkanRenderer::createCommandBuffer()
	{
		commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = getCommandPool();
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

		ASSERT(vkAllocateCommandBuffers(logicalDevice, &allocInfo, commandBuffers.data()) == VK_SUCCESS, "failed to allocate command buffers.");
	}

	void VulkanRenderer::resetCommandBuffer(const int bufferId) const
	{
		ASSERT(static_cast<size_t>(bufferId) < commandBuffers.size() && bufferId >= 0, "current frame number is larger than number of fences.");

		ASSERT(vkResetCommandBuffer(commandBuffers[bufferId], 0) == VK_SUCCESS, "Failed to reset command buffer.");
	}

	void VulkanRenderer::recordCommandBuffer(const VkCommandBuffer commandBufferToRecord, const uint32_t imageIndex) const
	{
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0;
		beginInfo.pInheritanceInfo = nullptr;

		ASSERT(vkBeginCommandBuffer(commandBufferToRecord, &beginInfo) == VK_SUCCESS, "failed to begin recording command buffer.");

		// Start the rendering pass
		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = renderPass;
		renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = swapChainDetails.swapChainExtent;

		std::array<VkClearValue, 2> clearValues{};
		clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
		clearValues[1].depthStencil = { 1.0f, 0 };

		renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
		renderPassInfo.pClearValues = clearValues.data();

		// Draw.
		vkCmdBeginRenderPass(commandBufferToRecord, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		// TODO: Maybe RAII initialization?
		// Bind the graphics pipeline.
		vkCmdBindPipeline(commandBufferToRecord, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(swapChainDetails.swapChainExtent.width);
		viewport.height = static_cast<float>(swapChainDetails.swapChainExtent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(commandBufferToRecord, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = swapChainDetails.swapChainExtent;
		vkCmdSetScissor(commandBufferToRecord, 0, 1, &scissor);

		const VkBuffer vertexBuffers[] = { globalBuffers.vertexBuffer };
		constexpr VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(commandBufferToRecord, 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(commandBufferToRecord, globalBuffers.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

		for (const auto& [mesh, transform] : models)
		{
			for (const auto& meshPart : mesh->meshParts)
			{
				// Bind the model's descriptor set.
				vkCmdBindDescriptorSets(commandBufferToRecord, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1,
					&meshPart.descriptorSets[currentFrame], 0, nullptr);

				vkCmdDrawIndexed(commandBufferToRecord,
					static_cast<uint32_t>(meshPart.indexCount),
					1,
					static_cast<uint32_t>(meshPart.indexOffset),
					static_cast<int32_t>(meshPart.vertexOffset),
					0);
			}
		}
		
		CORE->graphicsLibrary->renderDrawData(commandBufferToRecord);
		vkCmdEndRenderPass(commandBufferToRecord);
		ASSERT(vkEndCommandBuffer(commandBufferToRecord) == VK_SUCCESS, " failed to record command buffer.");
	}

	VkCommandBuffer VulkanRenderer::getCommandBuffer(const int bufferId) const
	{
		ASSERT(static_cast<size_t>(bufferId) < commandBuffers.size() && bufferId >= 0, "current frame number is larger than number of fences.");

		return commandBuffers[bufferId];
	}

	VkCommandPool VulkanRenderer::getCommandPool()
	{
		const std::thread::id threadId = std::this_thread::get_id();
		if (!threadCommandPools.contains(threadId))
		{
			threadCommandPools[threadId] = createCommandPool();		
		}
		
		return threadCommandPools[threadId];
	}
	
	VkCommandPool VulkanRenderer::createCommandPool() const
	{
		VkCommandPool newCommandPool;
		
		const auto [graphicsFamily, presentFamily] = findQueueFamilies(physicalDevice, surface);
		ASSERT(graphicsFamily.has_value(), "Graphics family is incomplete.");
		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = graphicsFamily.value();

		ASSERT(vkCreateCommandPool(logicalDevice, &poolInfo, nullptr, &newCommandPool) == VK_SUCCESS, "failed to create command pool.");
		return newCommandPool;
	}

	VkCommandBuffer VulkanRenderer::beginSingleTimeCommands()
	{
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = getCommandPool();
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers(logicalDevice, &allocInfo, &commandBuffer);

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(commandBuffer, &beginInfo);

		return commandBuffer;
	}

	void VulkanRenderer::endSingleTimeCommands(const VkCommandBuffer commandBuffer)
	{
		vkEndCommandBuffer(commandBuffer);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		threadSafeQueueSubmit(&submitInfo, nullptr);
		{
			std::lock_guard lock(graphicsQueueMutex);
			vkQueueWaitIdle(graphicsQueue);
		}
		vkFreeCommandBuffers(logicalDevice, getCommandPool(), 1, &commandBuffer);
	}

	void VulkanRenderer::createDepthResources()
	{
		const VkFormat depthFormat = findDepthFormat();

		createImage(swapChainDetails.swapChainExtent.width,
			swapChainDetails.swapChainExtent.height,
			1,
			msaaSamples,
			depthFormat,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory);

		depthImageView = createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);
	}

	VkFormat VulkanRenderer::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
	{
		std::optional<VkFormat> supportedFormat;
		for (VkFormat format : candidates) {
			VkFormatProperties props;
			vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

			if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
				supportedFormat = format;
			}
			else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
				supportedFormat = format;
			}
		}

		ASSERT(supportedFormat.has_value(), "failed to find supported format.");

		return supportedFormat.value();

	}

	VkFormat VulkanRenderer::findDepthFormat()
	{
		return findSupportedFormat(
			{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
		);
	}

	bool VulkanRenderer::hasStencilComponent(const VkFormat format)
	{
		return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
	}

	void VulkanRenderer::generateMipmaps(const Texture& texture, const VkFormat imageFormat, const int32_t texWidth, const int32_t texHeight)
	{
		// Check if image format supports linear blitting
		VkFormatProperties formatProperties;
		vkGetPhysicalDeviceFormatProperties(physicalDevice, imageFormat, &formatProperties);

		ASSERT(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT, "texture image format does not support linear blitting.");

		const VkCommandBuffer commandBuffer = beginSingleTimeCommands();

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image = texture.textureImage;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.subresourceRange.levelCount = 1;

		int32_t mipWidth = texWidth;
		int32_t mipHeight = texHeight;

		for (uint32_t i = 1; i < texture.maxMipLevels; i++) {
			barrier.subresourceRange.baseMipLevel = i - 1;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			vkCmdPipelineBarrier(commandBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
				0, nullptr,
				0, nullptr,
				1, &barrier);

			VkImageBlit blit{};
			blit.srcOffsets[0] = { 0, 0, 0 };
			blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
			blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.srcSubresource.mipLevel = i - 1;
			blit.srcSubresource.baseArrayLayer = 0;
			blit.srcSubresource.layerCount = 1;
			blit.dstOffsets[0] = { 0, 0, 0 };
			blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
			blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.dstSubresource.mipLevel = i;
			blit.dstSubresource.baseArrayLayer = 0;
			blit.dstSubresource.layerCount = 1;

			vkCmdBlitImage(commandBuffer,
				texture.textureImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				texture.textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &blit,
				VK_FILTER_LINEAR);

			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			vkCmdPipelineBarrier(commandBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
				0, nullptr,
				0, nullptr,
				1, &barrier);

			if (mipWidth > 1) mipWidth /= 2;
			if (mipHeight > 1) mipHeight /= 2;
		}

		barrier.subresourceRange.baseMipLevel = texture.maxMipLevels - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier);

		endSingleTimeCommands(commandBuffer);
	}

	void VulkanRenderer::createImage(const uint32_t width,
		const uint32_t height,
		const uint32_t numberOfMipLevels,
		const VkSampleCountFlagBits numberOfSamples,
		const VkFormat format,
		const VkImageTiling tiling,
		const VkImageUsageFlags usage,
		const VkMemoryPropertyFlags properties,
		VkImage& image,
		VkDeviceMemory& imageMemory) const
	{
		ASSERT(width != 0 && height != 0, "Attempted to create image with invalid dimensions");
		
		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = width;
		imageInfo.extent.height = height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = numberOfMipLevels;
		imageInfo.arrayLayers = 1;
		imageInfo.format = format;
		imageInfo.tiling = tiling;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = usage;
		imageInfo.samples = numberOfSamples;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		ASSERT(vkCreateImage(logicalDevice, &imageInfo, nullptr, &image) == VK_SUCCESS, "failed to create image.");

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(logicalDevice, image, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

		ASSERT(vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &imageMemory) == VK_SUCCESS, "failed to allocate image memory.");

		vkBindImageMemory(logicalDevice, image, imageMemory, 0);
	}

	void VulkanRenderer::transitionImageLayout(
		const VkImage image,
		[[maybe_unused]] VkFormat format,
		const VkImageLayout oldLayout,
		const VkImageLayout newLayout,
		const uint32_t mipLevels)
	{
		const VkCommandBuffer commandBuffer = beginSingleTimeCommands();

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = mipLevels;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;

		VkPipelineStageFlags sourceStage{};
		VkPipelineStageFlags destinationStage{};

		ASSERT((oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
			|| (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
			"unsupported layout transition.");

		if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}

		vkCmdPipelineBarrier(
			commandBuffer,
			sourceStage, destinationStage,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);

		endSingleTimeCommands(commandBuffer);
	}

	void VulkanRenderer::copyBufferToImage(const VkBuffer buffer, const VkImage image, const uint32_t width, const uint32_t height)
	{
		const VkCommandBuffer commandBuffer = beginSingleTimeCommands();

		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = {
			width,
			height,
			1
		};

		vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		endSingleTimeCommands(commandBuffer);
	}

	VkSampler VulkanRenderer::createTextureSampler(const uint32_t maxMipLevels) const
	{
		VkSampler textureSampler;
		
		VkPhysicalDeviceProperties properties{};
		vkGetPhysicalDeviceProperties(physicalDevice, &properties);

		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.anisotropyEnable = VK_TRUE;
		samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.minLod = 0;
		samplerInfo.maxLod = static_cast<float>(maxMipLevels);
		samplerInfo.mipLodBias = 0.0f; // Optional

		ASSERT(vkCreateSampler(logicalDevice, &samplerInfo, nullptr, &textureSampler) == VK_SUCCESS, "failed to create texture sampler.");

		return textureSampler;
	}

	void VulkanRenderer::createColorResources()
	{
		const VkFormat colorFormat = swapChainDetails.swapChainImageFormat;
		const auto test = swapChainDetails;
		createImage(swapChainDetails.swapChainExtent.width, swapChainDetails.swapChainExtent.height, 1, msaaSamples, colorFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, colorImage, colorImageMemory);
		colorImageView = createImageView(colorImage, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
	}

	void VulkanRenderer::createBufferManager()
	{
		// Combine all vertex and index data
		std::vector<math::Vertex> allVertices;
		std::vector<uint32_t> allIndices;

		for (auto& mesh : CORE->world.getStorage()->getMeshes())
		{
			for (auto& meshPart : mesh->meshParts)
			{
				meshPart.vertexOffset = allVertices.size();
				meshPart.indexOffset = allIndices.size();

				allVertices.insert(allVertices.end(), meshPart.vertices.begin(), meshPart.vertices.end());
				allIndices.insert(allIndices.end(), meshPart.indices.begin(), meshPart.indices.end());
			}
		}
		
		createVertexBuffer(allVertices);
		createIndexBuffer(allIndices);
		createUniformBuffer();
	}

	void VulkanRenderer::createVertexBuffer(const std::vector<math::Vertex>& vertices)
	{
		if (globalBuffers.vertexBuffer != VK_NULL_HANDLE)
		{
			frames[currentFrame].buffersToDelete.emplace_back(
			   globalBuffers.vertexBuffer, 
			   globalBuffers.vertexBufferMemory
			);
		}
		
		const VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
		
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		createBuffer(
			bufferSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer,
			stagingBufferMemory);

		// Fill vertex buffer data.
		void* data;
		vkMapMemory(logicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, vertices.data(), bufferSize);
		vkUnmapMemory(logicalDevice, stagingBufferMemory);

		createBuffer(
			bufferSize,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			globalBuffers.vertexBuffer,
			globalBuffers.vertexBufferMemory);
		
		copyBuffer(stagingBuffer, globalBuffers.vertexBuffer, bufferSize);
		vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);
		vkFreeMemory(logicalDevice, stagingBufferMemory, nullptr);
	}

	void VulkanRenderer::createBuffer(const VkDeviceSize size, const VkBufferUsageFlags usage, const VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) const
	{
		ASSERT(size > 0, "Attempt to create buffer with empty size.");
		
		// Create buffer structure.
		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		ASSERT(vkCreateBuffer(logicalDevice, &bufferInfo, nullptr, &buffer) == VK_SUCCESS, "failed to create buffer.");
		ASSERT(buffer != VK_NULL_HANDLE, "Buffer must be valid.");

		// Calculate memory requirements.
		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(logicalDevice, buffer, &memRequirements);

		// Allocate vertex buffer memory.
		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

		ASSERT(vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &bufferMemory) == VK_SUCCESS, "failed to allocate buffer memory.");

		vkBindBufferMemory(logicalDevice, buffer, bufferMemory, 0);
	}

	uint32_t VulkanRenderer::findMemoryType(const uint32_t typeFilter, const VkMemoryPropertyFlags properties) const
	{
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

		std::optional<uint32_t> memoryIndex;

		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
		{
			if (typeFilter & 1 << i && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				memoryIndex = i;
				break;
			}
		}

		ASSERT(memoryIndex.has_value(), "failed to find suitable memory type.");
		return memoryIndex.value();
	}

	void VulkanRenderer::copyBuffer(const VkBuffer srcBuffer, const VkBuffer dstBuffer, const VkDeviceSize size)
	{
		const VkCommandBuffer commandBuffer = beginSingleTimeCommands();

		VkBufferCopy copyRegion{};
		copyRegion.size = size;
		vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

		endSingleTimeCommands(commandBuffer);
	}

	void VulkanRenderer::createIndexBuffer(const std::vector<uint32_t>& indices)
	{
		if (globalBuffers.indexBuffer != VK_NULL_HANDLE)
		{
			frames[currentFrame].buffersToDelete.emplace_back(
			   globalBuffers.indexBuffer, 
			   globalBuffers.indexBufferMemory
			);
		}
		
		const VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		createBuffer(bufferSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer,
			stagingBufferMemory);

		void* data;
		vkMapMemory(logicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, indices.data(), (size_t)bufferSize);
		vkUnmapMemory(logicalDevice, stagingBufferMemory);

		createBuffer(bufferSize,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			globalBuffers.indexBuffer,
			globalBuffers.indexBufferMemory);

		copyBuffer(stagingBuffer, globalBuffers.indexBuffer, bufferSize);

		vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);
		vkFreeMemory(logicalDevice, stagingBufferMemory, nullptr);
	}

	void VulkanRenderer::createUniformBuffer()
	{
		// Global UBO
		constexpr VkDeviceSize globalUboSize = sizeof(math::GlobalUbo);

		globalUboBuffers.resize(MAX_FRAMES_IN_FLIGHT);
		globalUboMemory.resize(MAX_FRAMES_IN_FLIGHT);
		globalUboMapped.resize(MAX_FRAMES_IN_FLIGHT);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			createBuffer(
				globalUboSize,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				globalUboBuffers[i],
				globalUboMemory[i]);

			ASSERT(globalUboBuffers[i] != VK_NULL_HANDLE, "Global Buffer must be valid");
			vkMapMemory(logicalDevice, globalUboMemory[i], 0, globalUboSize, 0, &globalUboMapped[i]);
		}

		ASSERT(globalUboBuffers[0] != VK_NULL_HANDLE, "Global Buffer must be invalid");


		// Instance UBO
		constexpr VkDeviceSize instanceUboSize = sizeof(math::InstanceUbo);

		instanceUboBuffers.resize(MAX_FRAMES_IN_FLIGHT);
		instanceUboMemory.resize(MAX_FRAMES_IN_FLIGHT);
		instanceUboMapped.resize(MAX_FRAMES_IN_FLIGHT);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			createBuffer(instanceUboSize,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				instanceUboBuffers[i],
				instanceUboMemory[i]);

			ASSERT(instanceUboBuffers[i] != VK_NULL_HANDLE, "Instance Buffer must be valid");

			vkMapMemory(logicalDevice, instanceUboMemory[i], 0, instanceUboSize, 0, &instanceUboMapped[i]);
		}

		ASSERT(instanceUboBuffers[0] != VK_NULL_HANDLE, "Instance Buffer must be valid");

	}

	void VulkanRenderer::updateUniformBuffer(const uint32_t currentImage) const
	{
		const SpectatorCamera camera = CORE->world.getMainCamera();

		math::GlobalUbo globalUbo{};

		globalUbo.view = math::Matrix4x4::lookAt(
			camera.getPosition(),
			camera.getPosition() + camera.getForwardVector(),
			camera.getUpVector());

		globalUbo.projection = math::Matrix4x4::perspective(
			math::radians(45.0f),
			static_cast<float>(swapChainDetails.swapChainExtent.width) / static_cast<float>(swapChainDetails.swapChainExtent.height),
			Z_NEAR, Z_FAR);
		
		memcpy(globalUboMapped[currentImage], &globalUbo, sizeof(globalUbo));

		math::InstanceUbo instanceUbo{};
		instanceUbo.model = math::Matrix4x4();

		memcpy(instanceUboMapped[currentImage], &instanceUbo, sizeof(instanceUbo));

	}

	void VulkanRenderer::createDescriptorPool()
	{
		size_t numberOfMeshes = 1000;
		// for (const auto& [mesh, transform] : models)
		// {
		// 	numberOfMeshes += mesh->meshParts.size();
		// }
		
		std::array<VkDescriptorPoolSize, 2> poolSizes{};
		poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSizes[0].descriptorCount = static_cast<uint32_t>(numberOfMeshes * MAX_FRAMES_IN_FLIGHT) * 2;
		poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;	
		poolSizes[1].descriptorCount = static_cast<uint32_t>(numberOfMeshes * MAX_FRAMES_IN_FLIGHT) + IMAGE_SAMPLER_POOL_SIZE;
		
		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = static_cast<uint32_t>(numberOfMeshes * MAX_FRAMES_IN_FLIGHT) * 2;

		ASSERT(vkCreateDescriptorPool(logicalDevice, &poolInfo, nullptr, &descriptorPool) == VK_SUCCESS, "failed to create descriptor pool.");
	}

	void VulkanRenderer::createDescriptorSets(const std::shared_ptr<Mesh>& mesh) const
	{
		for (auto& meshPart : mesh->meshParts)
		{
			const std::vector layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
			VkDescriptorSetAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = descriptorPool;
			allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
			allocInfo.pSetLayouts = layouts.data();

			meshPart.descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
			ASSERT(vkAllocateDescriptorSets(logicalDevice, &allocInfo, meshPart.descriptorSets.data()) == VK_SUCCESS, "failed to allocate descriptor sets.");

			for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
			{
				VkDescriptorBufferInfo globalBufferInfo{};
				globalBufferInfo.buffer = globalUboBuffers[i];
				globalBufferInfo.offset = 0;
				globalBufferInfo.range = sizeof(math::GlobalUbo);
			
				VkDescriptorBufferInfo instanceBufferInfo{};
				instanceBufferInfo.buffer = instanceUboBuffers[i];
				instanceBufferInfo.offset = 0;
				instanceBufferInfo.range = sizeof(math::InstanceUbo);

				DEBUG_ASSERT(meshPart.texture, "Texture must exist for any mesh part.");

				VkDescriptorImageInfo imageInfo{};
				imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				imageInfo.imageView = meshPart.texture->textureImageView;
				imageInfo.sampler = meshPart.texture->textureSampler;
			
				std::array<VkWriteDescriptorSet, 3> descriptorWrites{};

				descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrites[0].dstSet = meshPart.descriptorSets[i];
				descriptorWrites[0].dstBinding = 0;
				descriptorWrites[0].dstArrayElement = 0;
				descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				descriptorWrites[0].descriptorCount = 1;
				descriptorWrites[0].pBufferInfo = &globalBufferInfo;

				descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrites[1].dstSet = meshPart.descriptorSets[i];
				descriptorWrites[1].dstBinding = 1;
				descriptorWrites[1].dstArrayElement = 0;
				descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				descriptorWrites[1].descriptorCount = 1;
				descriptorWrites[1].pBufferInfo = &instanceBufferInfo;

				descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrites[2].dstSet = meshPart.descriptorSets[i];
				descriptorWrites[2].dstBinding = 2;
				descriptorWrites[2].dstArrayElement = 0;
				descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				descriptorWrites[2].descriptorCount = 1;
				descriptorWrites[2].pImageInfo = &imageInfo;

				vkUpdateDescriptorSets(logicalDevice, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
			}
		}
		
	}

	void VulkanRenderer::createSyncObjects()
	{
		imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			ASSERT(vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) == VK_SUCCESS &&
				vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) == VK_SUCCESS &&
				vkCreateFence(logicalDevice, &fenceInfo, nullptr, &inFlightFences[i]) == VK_SUCCESS,
				"failed to create semaphores.");
		}
	}

	void VulkanRenderer::waitForFences() const
	{
		ASSERT(static_cast<size_t>(currentFrame) < inFlightFences.size() && currentFrame >= 0, "current frame number is larger than number of fences.");

		vkWaitForFences(logicalDevice, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
		vkResetFences(logicalDevice, 1, &inFlightFences[currentFrame]);
	}

	void VulkanRenderer::resetFences() const
	{
		ASSERT(static_cast<size_t>(currentFrame) < inFlightFences.size() && currentFrame >= 0, "current frame number is larger than number of fences.");

		vkResetFences(logicalDevice, 1, &inFlightFences[currentFrame]);
	}

	void VulkanRenderer::onResize()
	{
		framebufferResized = true;
	}

	VkSampleCountFlagBits VulkanRenderer::getMaxUsableSampleCount() const
	{
		VkPhysicalDeviceProperties physicalDeviceProperties;
		vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

		const VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
		if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
		if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
		if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
		if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
		if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
		if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

		return VK_SAMPLE_COUNT_1_BIT;
	}

	VkVertexInputBindingDescription VulkanRenderer::getBindingDescription()
	{
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(math::Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return bindingDescription;
	}

	std::array<VkVertexInputAttributeDescription, 3> VulkanRenderer::getAttributeDescriptions()
	{
		std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(math::Vertex, position);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(math::Vertex, color);

		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[2].offset = offsetof(math::Vertex, textureCoordinates);

		return attributeDescriptions;
	}

	void VulkanRenderer::cleanupFrameResources()
	{
		// Destroy buffers scheduled for deletion from N frames ago
		const uint32_t frameToClean = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    
		for (auto& [buffer, memory] : frames[frameToClean].buffersToDelete)
		{
			vkDestroyBuffer(logicalDevice, buffer, nullptr);
			vkFreeMemory(logicalDevice, memory, nullptr);
		}
		frames[frameToClean].buffersToDelete.clear();
	}
}
