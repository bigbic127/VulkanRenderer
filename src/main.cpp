#include <iostream>

#include <vulkan/vulkan_raii.hpp>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

const std::vector<char const*> validationLayers = {
	"VK_LAYER_KHRONOS_validation"};
#ifdef NDEBUG
	constexpr bool enableValidationLayers = false;
#else
	constexpr bool enableValidationLayers = true;
#endif


class TriangleVulkan
{
	public:
		void Run(){
			if(InitGLFW())
			{
				InitVulkan();
				Loop();
				Destroy();
			}
		}
		bool InitGLFW(){
			if(!glfwInit())
				return false;
			glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
			window = glfwCreateWindow(800, 600, "Vulkan Triangle", nullptr, nullptr);
			if(window == nullptr){
				glfwTerminate();
				return false;
			}
			return true;
		}
		void InitVulkan(){
			constexpr vk::ApplicationInfo appInfo{	"Vulkan Triangle",
													vk::makeVersion(1,0,0),
													"No Engine",
													vk::makeVersion(1,0,0),
													vk::ApiVersion14};
			//Get the required layers
			std::vector<char const*> requiredLayers;
			if(enableValidationLayers)
				requiredLayers.assign(validationLayers.begin(), validationLayers.end());
			auto layerProperties = context.enumerateInstanceLayerProperties();
			auto unSupportedLayerIt = std::ranges::find_if(requiredLayers, [&layerProperties](auto const& requiredLayer){return std::ranges::none_of(layerProperties, [requiredLayer](auto const& layerProperty){return strcmp(layerProperty.layerName, requiredLayer) == 0;});});
			if(unSupportedLayerIt != requiredLayers.end())
				throw std::runtime_error("Required layer not supported: " + std::string(*unSupportedLayerIt));
			auto requiredExtensions = GetRequiredInstanceExtensions();
			//Check if the required extensions are supported by the Vulkan implementation
			auto extensionProperties = context.enumerateInstanceExtensionProperties();
			auto unSupportedPropertyIt = std::ranges::find_if(requiredExtensions,[&extensionProperties](auto const& requiredExtension){return std::ranges::none_of(extensionProperties,[requiredExtension](auto const& extensionProperty){return strcmp(extensionProperty.extensionName, requiredExtension) == 0;});});
			if(unSupportedPropertyIt != requiredExtensions.end())
				throw std::runtime_error("Required extension not supported: " + std::string(*unSupportedPropertyIt));
			vk::InstanceCreateInfo createInfo{	vk::InstanceCreateFlags(),
												&appInfo,
												static_cast<uint32_t>(requiredLayers.size()),
												requiredLayers.data(),
												static_cast<uint32_t>(requiredExtensions.size()),
												requiredExtensions.data()};
			instance = vk::raii::Instance(context, createInfo);
			// 
			SetupDebugMessenger();
		}
		void Loop()
		{
			while(!glfwWindowShouldClose(window)){
				glfwPollEvents();
			}
		}
		void Destroy(){
			glfwDestroyWindow(window);
			glfwTerminate();
		}
		std::vector<const char*> GetRequiredInstanceExtensions(){
			uint32_t glfwExtensionCount = 0;
			auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
			std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
			if(enableValidationLayers)
				extensions.push_back(vk::EXTDebugUtilsExtensionName);
			return extensions;
		}
		void SetupDebugMessenger(){
			if (!enableValidationLayers)
				return;

			vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
																vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
																vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
			vk::DebugUtilsMessageTypeFlagsEXT     messageTypeFlags(
				vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
			vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT{};
			debugUtilsMessengerCreateInfoEXT.messageSeverity = severityFlags;
			debugUtilsMessengerCreateInfoEXT.messageType = messageTypeFlags;
			debugUtilsMessengerCreateInfoEXT.pfnUserCallback = &TriangleVulkan::debugCallback;
			debugMessenger = instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
		}
		static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type, const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData, void *)
		{
			if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError || severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
			{
				std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;
			}

			return vk::False;
		}

	private:
		GLFWwindow* window = nullptr;
		vk::raii::Context context;
		vk::raii::Instance instance = nullptr;
		vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;

};

int main()
{
	TriangleVulkan app;
	app.Run();
	return -1;
}