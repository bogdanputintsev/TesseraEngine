#include "Application.h"

#include "engine/Event.h"
#include "engine/input/Input.h"
#include "services/platform/Platform.h"
#include "services/graphics/imgui/ImGuiLibrary.h"
#include "services/renderer/vulkan/VulkanRenderer.h"
#include "services/Services.h"
#include "services/threading/ThreadPool.h"
#include "services/world/World.h"

namespace parus
{
	void Application::init()
	{
		registerServices();
		registerEvents();
		
		applicationInfo.readAll();
		
		Services::get<Platform>()->init();
		Services::get<ThreadPool>()->init();
		Services::get<vulkan::VulkanRenderer>()->init();
		Services::get<imgui::ImGuiLibrary>()->init();

		isRunning = true;
	}

	void Application::registerServices()
	{
		const auto platform = std::make_shared<Platform>();
		const auto graphicsLibrary = std::make_shared<imgui::ImGuiLibrary>();
		const auto renderer = std::make_shared<vulkan::VulkanRenderer>();
		const auto eventSystem = std::make_shared<EventSystem>();
		const auto inputSystem = std::make_shared<Input>();
		const auto world = std::make_shared<World>();
		const auto threadPool = std::make_shared<ThreadPool>();
		
		Services::registerService(platform.get(), platform);
		Services::registerService(graphicsLibrary.get(), graphicsLibrary);
		Services::registerService(renderer.get(), renderer);
		Services::registerService(eventSystem.get(), eventSystem);
		Services::registerService(inputSystem.get(), inputSystem);
		Services::registerService(world.get(), world);
		Services::registerService(threadPool.get(), threadPool);
	}

	void Application::registerEvents()
	{
		REGISTER_EVENT(EventType::EVENT_KEY_RELEASED, [](const KeyButton keyReleased)
		{
			if (keyReleased == KeyButton::KEY_ESCAPE)
			{
				FIRE_EVENT(EventType::EVENT_APPLICATION_QUIT, 0);
			}
		});
		
		REGISTER_EVENT(EventType::EVENT_APPLICATION_QUIT, [&]([[maybe_unused]]const int exitCode)
		{
			isRunning = false;
		});
	}

	void Application::loop()
	{
		auto lastFrameTime = std::chrono::high_resolution_clock::now();

		while (isRunning)
		{
			const auto currentFrameTime = std::chrono::high_resolution_clock::now();
			const std::chrono::duration<float> deltaTimeDuration = currentFrameTime - lastFrameTime;
			const float deltaTime = deltaTimeDuration.count();
			lastFrameTime = currentFrameTime;

			Services::get<World>()->tick(deltaTime);
			Services::get<Platform>()->getMessages();

			if (!Services::get<Platform>()->getWindowInfo().isMinimized)
			{
				Services::get<imgui::ImGuiLibrary>()->drawFrame();
				Services::get<vulkan::VulkanRenderer>()->drawFrame();
			}
		}

		Services::get<vulkan::VulkanRenderer>()->deviceWaitIdle();
	}

	void Application::clean()
	{
		isRunning = false;
		
		Services::get<imgui::ImGuiLibrary>()->clean();
		Services::get<vulkan::VulkanRenderer>()->clean();
		Services::get<Platform>()->clean();
	}

}
