#include "ImGuiLibrary.h"

#include "third-party/imgui/backends/imgui_impl_vulkan.h"
#include "services/platform/Platform.h"
#include "third-party/imgui/backends/imgui_impl_win32.h"
#include "engine/input/Input.h"
#include "services/renderer/vulkan/VulkanRenderer.h"
#include "services/Services.h"
#include "third-party/imgui/imgui.h"

#include "engine/EngineCore.h"
#include "engine/Event.h"

namespace parus::imgui
{
	
	void ImGuiLibrary::init()
	{
		const auto& vulkanContext = Services::get<vulkan::VulkanRenderer>();
		const auto& windowsContext = Services::get<Platform>()->platformState;

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls

		ImGui::StyleColorsDark();
		
		//this initializes imgui for SDL
		ImGui_ImplWin32_Init(windowsContext.hwnd);

		//this initializes imgui for Vulkan
		ImGui_ImplVulkan_InitInfo initInfo = {};
		initInfo.Instance = vulkanContext->instance;
		initInfo.PhysicalDevice = vulkanContext->physicalDevice;
		initInfo.Device = vulkanContext->logicalDevice;
		initInfo.Queue = vulkanContext->graphicsQueue;
		initInfo.DescriptorPool = vulkanContext->descriptorPool;
		initInfo.RenderPass = vulkanContext->renderPass;
		initInfo.MinImageCount = 2;
		initInfo.ImageCount = 2;
		initInfo.MSAASamples = vulkanContext->msaaSamples;

		ImGui_ImplVulkan_Init(&initInfo);

		REGISTER_EVENT(EventType::EVENT_MOUSE_BUTTON_PRESSED, [](const parus::MouseButton button)
		{
			switch (button)
			{
				case MouseButton::BUTTON_LEFT:
				{
					ImGui::GetIO().MouseDown[0] = true;
					break;
				}
				case MouseButton::BUTTON_RIGHT:
				{
					ImGui::GetIO().MouseDown[1] = true;
					break;
				}
				case MouseButton::BUTTON_MIDDLE:
				{
					ImGui::GetIO().MouseDown[2] = true;
					break;
				}
			}
		});

		REGISTER_EVENT(EventType::EVENT_MOUSE_BUTTON_RELEASED, [](const parus::MouseButton button)
		{
			switch (button)
			{
				case MouseButton::BUTTON_LEFT:
				{
					ImGui::GetIO().MouseDown[0] = false;
					break;
				}
				case MouseButton::BUTTON_RIGHT:
				{
					ImGui::GetIO().MouseDown[1] = false;
					break;
				}
				case MouseButton::BUTTON_MIDDLE:
				{
					ImGui::GetIO().MouseDown[2] = false;
					break;
				}
			}
		});

		REGISTER_EVENT(EventType::EVENT_MOUSE_WHEEL, [](const int wheelDelta)
		{
			ImGui::GetIO().MouseWheel += static_cast<float>(wheelDelta);
		});

		REGISTER_EVENT(EventType::EVENT_MOUSE_MOVED, [](const int mouseX, const int mouseY)
		{
			ImGui::GetIO().MousePos.x = static_cast<float>(mouseX);
			ImGui::GetIO().MousePos.y = static_cast<float>(mouseY);
		});

		REGISTER_EVENT(EventType::EVENT_KEY_PRESSED, [](const KeyButton key)
		{
			ImGui::GetIO().AddKeyEvent(getImGuiKeyCode(key), true);
		});

		REGISTER_EVENT(EventType::EVENT_KEY_RELEASED, [](const KeyButton key)
		{
			ImGui::GetIO().AddKeyEvent(getImGuiKeyCode(key), false);
		});

		REGISTER_EVENT(EventType::EVENT_CHAR_INPUT, [](const char inputChar)
		{
			ImGui::GetIO().AddInputCharacter(inputChar);
		});

		consoleGui.registerEvents();
	}
	
	void ImGuiLibrary::renderDrawData(const VkCommandBuffer cmd)
	{
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
	}

	void ImGuiLibrary::drawFrame()
	{
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		draw();

		ImGui::Render();
	}

	void ImGuiLibrary::draw()
	{
		consoleGui.draw();
	}

	void ImGuiLibrary::handleMinimization()
	{
	}

	std::vector<const char*> ImGuiLibrary::getRequiredExtensions() const
	{
		return {"VK_KHR_surface", "VK_KHR_win32_surface"};
	}

	void ImGuiLibrary::clean()
	{
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}

	ImGuiKey ImGuiLibrary::getImGuiKeyCode(const ::parus::KeyButton keyTest)
	{
		switch (keyTest)
		{
		case KeyButton::KEY_BACKSPACE:
			return ImGuiKey_Backspace;
		case KeyButton::KEY_ENTER:
			return ImGuiKey_Enter;
		case KeyButton::KEY_TAB:
			return ImGuiKey_Tab;
		case KeyButton::KEY_SHIFT:
			return ImGuiKey_LeftShift;
		case KeyButton::KEY_CONTROL:
			return ImGuiKey_LeftCtrl;
		case KeyButton::KEY_PAUSE:
			return ImGuiKey_Pause;
		case KeyButton::KEY_CAPITAL:
			return ImGuiKey_CapsLock;
		case KeyButton::KEY_ESCAPE:
			return ImGuiKey_Escape;
		case KeyButton::KEY_SPACE:
			return ImGuiKey_Space;
		case KeyButton::KEY_END:
			return ImGuiKey_End;
		case KeyButton::KEY_HOME:
			return ImGuiKey_Home;
		case KeyButton::KEY_LEFT:
			return ImGuiKey_LeftArrow;
		case KeyButton::KEY_UP:
			return ImGuiKey_UpArrow;
		case KeyButton::KEY_RIGHT:
			return ImGuiKey_RightArrow;
		case KeyButton::KEY_DOWN:
			return ImGuiKey_DownArrow;
		case KeyButton::KEY_PRINT:
			return ImGuiKey_PrintScreen;
		case KeyButton::KEY_INSERT:
			return ImGuiKey_Insert;
		case KeyButton::KEY_DELETE:
			return ImGuiKey_Delete;
		case KeyButton::KEY_A:
			return ImGuiKey_A;
		case KeyButton::KEY_B:
			return ImGuiKey_B;
		case KeyButton::KEY_C:
			return ImGuiKey_C;
		case KeyButton::KEY_D:
			return ImGuiKey_D;
		case KeyButton::KEY_E:
			return ImGuiKey_E;
		case KeyButton::KEY_F:
			return ImGuiKey_F;
		case KeyButton::KEY_G:
			return ImGuiKey_G;
		case KeyButton::KEY_H:
			return ImGuiKey_H;
		case KeyButton::KEY_I:
			return ImGuiKey_I;
		case KeyButton::KEY_J:
			return ImGuiKey_J;
		case KeyButton::KEY_K:
			return ImGuiKey_K;
		case KeyButton::KEY_L:
			return ImGuiKey_L;
		case KeyButton::KEY_M:
			return ImGuiKey_M;
		case KeyButton::KEY_N:
			return ImGuiKey_N;
		case KeyButton::KEY_O:
			return ImGuiKey_O;
		case KeyButton::KEY_P:
			return ImGuiKey_P;
		case KeyButton::KEY_Q:
			return ImGuiKey_Q;
		case KeyButton::KEY_R:
			return ImGuiKey_R;
		case KeyButton::KEY_S:
			return ImGuiKey_S;
		case KeyButton::KEY_T:
			return ImGuiKey_T;
		case KeyButton::KEY_U:
			return ImGuiKey_T;
		case KeyButton::KEY_V:
			return ImGuiKey_V;
		case KeyButton::KEY_W:
			return ImGuiKey_W;
		case KeyButton::KEY_X:
			return ImGuiKey_X;
		case KeyButton::KEY_Y:
			return ImGuiKey_Y;
		case KeyButton::KEY_Z:
			return ImGuiKey_Z;
		case KeyButton::KEY_LWIN:
			return ImGuiKey_LeftSuper;
		case KeyButton::KEY_RWIN:
			return ImGuiKey_RightSuper;
		case KeyButton::KEY_NUMPAD0:
			return ImGuiKey_Keypad0;
		case KeyButton::KEY_NUMPAD1:
			return ImGuiKey_Keypad1;
		case KeyButton::KEY_NUMPAD2:
			return ImGuiKey_Keypad2;
		case KeyButton::KEY_NUMPAD3:
			return ImGuiKey_Keypad3;
		case KeyButton::KEY_NUMPAD4:
			return ImGuiKey_Keypad4;
		case KeyButton::KEY_NUMPAD5:
			return ImGuiKey_Keypad5;
		case KeyButton::KEY_NUMPAD6:
			return ImGuiKey_Keypad6;
		case KeyButton::KEY_NUMPAD7:
			return ImGuiKey_Keypad7;
		case KeyButton::KEY_NUMPAD8:
			return ImGuiKey_Keypad8;
		case KeyButton::KEY_NUMPAD9:
			return ImGuiKey_Keypad9;
		case KeyButton::KEY_MULTIPLY:
			return ImGuiKey_KeypadMultiply;
		case KeyButton::KEY_ADD:
			return ImGuiKey_KeypadAdd;
		case KeyButton::KEY_SEPARATOR:
			return ImGuiKey_Slash;
		case KeyButton::KEY_SUBTRACT:
			return ImGuiKey_KeypadSubtract;
		case KeyButton::KEY_DECIMAL:
			return ImGuiKey_KeypadDecimal;
		case KeyButton::KEY_DIVIDE:
			return ImGuiKey_KeypadDivide;
		case KeyButton::KEY_F1:
			return ImGuiKey_F1;
		case KeyButton::KEY_F2:
			return ImGuiKey_F2;
		case KeyButton::KEY_F3:
			return ImGuiKey_F3;
		case KeyButton::KEY_F4:
			return ImGuiKey_F4;
		case KeyButton::KEY_F5:
			return ImGuiKey_F5;
		case KeyButton::KEY_F6:
			return ImGuiKey_F6;
		case KeyButton::KEY_F7:
			return ImGuiKey_F7;
		case KeyButton::KEY_F8:
			return ImGuiKey_F8;
		case KeyButton::KEY_F9:
			return ImGuiKey_F9;
		case KeyButton::KEY_F10:
			return ImGuiKey_F10;
		case KeyButton::KEY_F11:
			return ImGuiKey_F11;
		case KeyButton::KEY_F12:
			return ImGuiKey_F12;
		case KeyButton::KEY_F13:
			return ImGuiKey_F13;
		case KeyButton::KEY_F14:
			return ImGuiKey_F14;
		case KeyButton::KEY_F15:
			return ImGuiKey_F15;
		case KeyButton::KEY_F16:
			return ImGuiKey_F16;
		case KeyButton::KEY_F17:
			return ImGuiKey_F17;
		case KeyButton::KEY_F18:
			return ImGuiKey_F18;
		case KeyButton::KEY_F19:
			return ImGuiKey_F19;
		case KeyButton::KEY_F20:
			return ImGuiKey_F20;
		case KeyButton::KEY_F21:
			return ImGuiKey_F21;
		case KeyButton::KEY_F22:
			return ImGuiKey_F22;
		case KeyButton::KEY_F23:
			return ImGuiKey_F23;
		case KeyButton::KEY_F24:
			return ImGuiKey_F24;
		case KeyButton::KEY_NUMLOCK:
			return ImGuiKey_NumLock;
		case KeyButton::KEY_SCROLL:
			return ImGuiKey_ScrollLock;
		case KeyButton::KEY_NUMPAD_EQUAL:
			return ImGuiKey_KeypadEqual;
		case KeyButton::KEY_LSHIFT:
			return ImGuiKey_LeftShift;
		case KeyButton::KEY_RSHIFT:
			return ImGuiKey_RightShift;
		case KeyButton::KEY_LCONTROL:
			return ImGuiKey_LeftCtrl;
		case KeyButton::KEY_RCONTROL:
			return ImGuiKey_RightCtrl;
		case KeyButton::KEY_LMENU:
			return ImGuiKey_Menu;
		case KeyButton::KEY_RMENU:
			return ImGuiKey_Menu;
		case KeyButton::KEY_SEMICOLON:
			return ImGuiKey_Semicolon;
		case KeyButton::KEY_PLUS:
			return ImGuiKey_KeypadAdd;
		case KeyButton::KEY_COMMA:
			return ImGuiKey_Comma;
		case KeyButton::KEY_MINUS:
			return ImGuiKey_Minus;
		case KeyButton::KEY_PERIOD:
			return ImGuiKey_Period;
		case KeyButton::KEY_SLASH:
			return ImGuiKey_Slash;
		case KeyButton::KEY_GRAVE:
			return ImGuiKey_GraveAccent;
		case KeyButton::KEY_CONVERT:
		case KeyButton::KEY_NONCONVERT:
		case KeyButton::KEY_ACCEPT:
		case KeyButton::KEY_MODECHANGE:
		case KeyButton::KEY_PRIOR:
		case KeyButton::KEY_NEXT:
		case KeyButton::KEY_SELECT:
		case KeyButton::KEY_SNAPSHOT:
		case KeyButton::KEY_HELP:
		case KeyButton::KEY_APPS:
		case KeyButton::KEY_SLEEP:
			break;
		}
		
		return ImGuiKey_None;
	}
}
