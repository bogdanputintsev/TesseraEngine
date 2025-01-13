#pragma once
#include <optional>
#include <vector>

#include "entities/Vertex.h"
#include "renderer/Renderer.h"
#include "utils/TesseraLog.h"

namespace tessera::vulkan
{
	struct SwapChainSupportDetails
	{
		VkSurfaceCapabilitiesKHR capabilities{};
		std::vector<VkSurfaceFormatKHR> formats{};
		std::vector<VkPresentModeKHR> presentModes{};

		[[nodiscard]] bool isComplete() const { return !formats.empty() && !presentModes.empty(); }
	};

	struct SwapChainImageDetails
	{
		VkFormat swapChainImageFormat{};
		VkExtent2D swapChainExtent{};
		std::vector<VkImage> swapChainImages{};
	};

	struct QueueFamilyIndices
	{
		std::optional<uint32_t> graphicsFamily;
		std::optional<uint32_t> presentFamily;

		[[nodiscard]] bool isComplete() const { return graphicsFamily.has_value() && presentFamily.has_value(); }
	};

	struct VulkanContext final
	{
		// Instance
		void createInstance();
		static void checkValidationLayerSupport();
		static bool validationLayersAreEnabled();
		static std::vector<const char*> getValidationLayers();
		static void checkIfAllGlsfRequiredExtensionsAreSupported();
		void destroyDebugUtilsMessengerExt(VkDebugUtilsMessengerEXT debugMessengerToDestroy,
			const VkAllocationCallbacks* pAllocator) const;
		static std::vector<const char*> getRequiredInstanceExtensions();
		static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
			[[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT messageType,
			const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
			[[maybe_unused]] void* pUserData);
		static void populate(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
		static LogType getLogType(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity);

		// DebugManager
		void createDebugManager();
		VkResult createDebugUtilsMessengerExt(const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
			const VkAllocationCallbacks* pAllocator,
			VkDebugUtilsMessengerEXT* pDebugMessenger) const;

		// Surface.
		void createSurface();

		// Devices.
		void createDevices();
		void pickAnySuitableDevice();
		static bool isDeviceSuitable(const VkPhysicalDevice& device, const VkSurfaceKHR& surface);
		static SwapChainSupportDetails querySwapChainSupport(const VkPhysicalDevice& device, const VkSurfaceKHR& surface);

		// Queues
		void createQueues();

		// SwapChain
		void createSwapChain();
		static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
		static VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
		static VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
		[[nodiscard]] std::optional<uint32_t> acquireNextImage();
		void recreateSwapChain();
		void cleanupSwapChain();

		// Image View
		void createImageView();

		// Render Pass
		void createRenderPass();

		// Descriptor Set Layout
		void createDescriptorSetLayout();

		// Graphics Pipeline
		void createGraphicsPipeline();
		static VkShaderModule createShaderModule(const std::vector<char>& code, const VkDevice& device);


		// Framebuffer
		void createFramebuffer();

		// Command buffer
		void createCommandBuffer();
		void resetCommandBuffer(const int bufferId) const;
		void recordCommandBuffer(VkCommandBuffer commandBufferToRecord, uint32_t imageIndex) const;
		[[nodiscard]] VkCommandBuffer getCommandBuffer(const int bufferId) const;
		[[nodiscard]] VkCommandPool getCommandPool() const { return commandPool; }

		// Command pool
		void createCommandPool();

		// Buffer manager
		void createBufferManager();
		void createVertexBuffer();
		void createBuffer(const VkDeviceSize size, const VkBufferUsageFlags usage, const VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) const;
		[[nodiscard]] uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
		void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) const;
		void createIndexBuffer();
		void createUniformBuffer();
		void updateUniformBuffer(uint32_t currentImage);

		void createDescriptorPool();
		void createDescriptorSets();

		// Sync objects
		void createSyncObjects();
		void waitForFences() const;
		void resetFences() const;

		VkInstance instance = VK_NULL_HANDLE;
		VkDebugUtilsMessengerEXT debugMessenger = nullptr;
		VkSurfaceKHR surface = VK_NULL_HANDLE;
		VkDevice logicalDevice = VK_NULL_HANDLE;
		VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
		VkQueue graphicsQueue = VK_NULL_HANDLE;
		VkQueue presentQueue = VK_NULL_HANDLE;
		VkSwapchainKHR swapChain = VK_NULL_HANDLE;
		SwapChainImageDetails swapChainDetails{};
		std::vector<VkImageView> swapChainImageViews{};
		VkDescriptorPool descriptorPool;
		std::vector<VkDescriptorSet> descriptorSets;
		VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
		VkRenderPass renderPass = VK_NULL_HANDLE;
		VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
		VkPipeline graphicsPipeline = VK_NULL_HANDLE;
		VkShaderModule vertexShaderModule = VK_NULL_HANDLE;
		VkShaderModule fragmentShaderModule = VK_NULL_HANDLE;
		std::vector<VkFramebuffer> swapChainFramebuffers{};
		std::vector<VkCommandBuffer> commandBuffers{};
		VkCommandPool commandPool = VK_NULL_HANDLE;
		VkBuffer vertexBuffer = VK_NULL_HANDLE;
		VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
		VkBuffer indexBuffer = VK_NULL_HANDLE;
		VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;
		std::vector<VkBuffer> uniformBuffers{};
		std::vector<VkDeviceMemory> uniformBuffersMemory{};
		std::vector<void*> uniformBuffersMapped{};
		std::vector<VkSemaphore> imageAvailableSemaphores{};
		std::vector<VkSemaphore> renderFinishedSemaphores{};
		std::vector<VkFence> inFlightFences{};

		int currentFrame = 0;
		bool framebufferResized = false;

		// TODO: Temporary.
		const std::vector<Vertex> vertices = {
			{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
			{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
			{{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
			{{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
		};

		const std::vector<uint16_t> indices = {
			0, 1, 2, 2, 3, 0
		};

		static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
	};

	class VulkanRenderer final : public Renderer
	{
	public:
		void init() override;
		void clean() override;
		void drawFrame() override;
		void deviceWaitIdle() override;
		void onResize();
	private:
		VulkanContext context;
	};

}

