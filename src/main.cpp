#include <stdexcept>
#include <iostream>

#include <vulkan/vulkan_raii.hpp>

#define GLFW_INCLUDE_VLUKAN
#include <GLFW/glfw3.h>

int main()
{
	if(!glfwInit())
		return -1;
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	GLFWwindow* window = glfwCreateWindow(800, 600, "Vulkan Renderer", nullptr, nullptr);
	if(window == nullptr)
	{
		glfwTerminate();
		return -1;
	}
	vk::ApplicationInfo appInfo;
	appInfo.pApplicationName = "Vulkan Renderer";
	appInfo.applicationVersion = VK_MAKE_VERSION(1,0,0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1,0,0);
	appInfo.apiVersion = VK_API_VERSION_1_4;

	vk::raii::Context context;
	uint32_t glfwExtensionCount = 0;
	auto glfwExtension = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	auto extensionProperties = context.enumerateInstanceExtensionProperties();
	for(uint32_t i = 0; i < glfwExtensionCount; ++i)
	{
		if(std::ranges::none_of(extensionProperties,
								[glfwExtension = glfwExtension[i]](auto const &extensionProperty){return strcmp(extensionProperty.extensionName, glfwExtension) == 0;}))
		{
			throw std::runtime_error("Required GLFW extension not supported: " + std::string(glfwExtension[i]));
		}
	}
	vk::InstanceCreateInfo createInfo;
	createInfo.pApplicationInfo = &appInfo;
	createInfo.enabledExtensionCount = glfwExtensionCount;
	createInfo.ppEnabledExtensionNames = glfwExtension;

	vk::raii::Instance instance = vk::raii::Instance(context, createInfo);
	while(!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
	}
	glfwDestroyWindow(window);
	glfwTerminate();
	return -1;
}