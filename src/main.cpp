#include <iostream>
#include <fstream>

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

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

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
			glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
			window = glfwCreateWindow(800, 600, "Vulkan Triangle", nullptr, nullptr);
			if(window == nullptr){
				glfwTerminate();
				return false;
			}
			glfwSetWindowUserPointer(window, this);
			glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
			return true;
		}
		static void framebufferResizeCallback(GLFWwindow* window, int w, int h)
		{
			auto app				= reinterpret_cast<TriangleVulkan*>(glfwGetWindowUserPointer(window));
			app->framebufferResized = true;
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
			//ValidationLayers
			SetupDebugMessenger();
			//Surface
			CreateSurface();
			//PhsicalDevice
			SetupPhysicalDevice();
			//LogicalDevice and Queue
			CreateLogicalDevice();
			//SwapChain
			CreateSwapChain();
			//ImageView
			CreateImageViews();
			//GraphicsPipeline
			CreateGraphicsPipeline();
			//Command
			CreateCommandPool();
			CreateCommandBuffer();
			//SyncObjects
			CreateSyncObjects();
		}
		void Loop()
		{
			while(!glfwWindowShouldClose(window)){
				glfwPollEvents();
				DrawFrame();
			}
			device.waitIdle();
		}
		void DrawFrame()
		{
			auto fenceResult = device.waitForFences(*inFlightFence[frameIndex], vk::True, UINT64_MAX);
			if(fenceResult != vk::Result::eSuccess)
				throw std::runtime_error("failed to wait for fence!");
			auto[result, imageIndex] = swapChain.acquireNextImage(UINT64_MAX, *presentCompleteSemaphores[frameIndex], nullptr);
			if(result == vk::Result::eErrorOutOfDateKHR)
			{
				ReCreateSwapChain();
				return;
			}
			else if(result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
			{
				assert(result != vk::Result::eTimeout || result == vk::Result::eNotReady);
				throw std::runtime_error("failed to acquire swap chain image!");
			}
			device.resetFences(*inFlightFence[frameIndex]);
			commandBuffers[frameIndex].reset();
			RecordCommandBuffer(imageIndex);
			vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
			vk::SubmitInfo submitInfo;
			submitInfo.waitSemaphoreCount 		= 1;
			submitInfo.pWaitSemaphores			= &*presentCompleteSemaphores[frameIndex];
			submitInfo.pWaitDstStageMask		= &waitDestinationStageMask;
			submitInfo.commandBufferCount 		= 1;
			submitInfo.pCommandBuffers			= &*commandBuffers[frameIndex];
			submitInfo.signalSemaphoreCount		= 1;
			submitInfo.pSignalSemaphores		= &*renderFinishedSemaphores[imageIndex];
			queue.submit(submitInfo, *inFlightFence[frameIndex]);

			vk::PresentInfoKHR presentInfoKHR;
			presentInfoKHR.waitSemaphoreCount 	= 1;
			presentInfoKHR.pWaitSemaphores		= &*renderFinishedSemaphores[imageIndex];
			presentInfoKHR.swapchainCount		= 1;
			presentInfoKHR.pSwapchains			= &*swapChain;
			presentInfoKHR.pImageIndices		= &imageIndex;
			
			result = queue.presentKHR(presentInfoKHR);
			if((result == vk::Result::eSuboptimalKHR) || (result == vk::Result::eErrorOutOfDateKHR) || framebufferResized)
			{
				framebufferResized =false;
				ReCreateSwapChain();
			}
			else
			{
				assert(result == vk::Result::eSuccess);
			}
			frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
		}
		void Destroy(){
			glfwDestroyWindow(window);
			glfwTerminate();
		}
		void CleanupSwapChain()
		{
			swapChainImageViews.clear();
			swapChain = nullptr;
		}
		void ReCreateSwapChain()
		{
			int w=0, h = 0;
			glfwGetFramebufferSize(window, &w, &h);
			while(w == 0 || h == 0)
			{
				glfwGetFramebufferSize(window, &w, &h);
				glfwWaitEvents();
			}
			device.waitIdle();
			CleanupSwapChain();
			CreateSwapChain();
			CreateImageViews();
		}
		//
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
		void CreateSurface(){
			VkSurfaceKHR _surface;
			if(glfwCreateWindowSurface(*instance, window, nullptr, &_surface) !=0 )
			{
				throw std::runtime_error("failed to create window surface!");
			}
			surface = vk::raii::SurfaceKHR(instance, _surface);
		}
		void SetupPhysicalDevice(){
			std::vector<vk::raii::PhysicalDevice> devices = instance.enumeratePhysicalDevices();
			//람다식을 사용해서 그래픽카드와 큐패밀리 확인
			const auto devIter = std::ranges::find_if(devices,[&](auto const& device){
				if(enableValidationLayers){
					auto props = device.getProperties();
					std::cerr<<"Device: " << props.deviceName << std::endl;;

					std::cerr<<"Vulkan Version: " 	<<VK_VERSION_MAJOR(props.apiVersion)<<"."
													<<VK_VERSION_MINOR(props.apiVersion)<<"."
													<<VK_VERSION_PATCH(props.apiVersion)<<std::endl;
					//
					auto queProps = device.getQueueFamilyProperties();
					int count = 0;
					std::ranges::for_each(queProps,[&count](auto const& queProp){
						std::string flags;
						if(queProp.queueFlags & vk::QueueFlagBits::eGraphics)
							flags += "Graphics ";
						if(queProp.queueFlags & vk::QueueFlagBits::eCompute)
							flags += "Compute ";
						if(queProp.queueFlags & vk::QueueFlagBits::eTransfer)
							flags += "Transfer ";
						std::cerr<<"Queue Family["<< count <<"] " <<"Count: " << queProp.queueCount << " | Flags: " << flags << std::endl;
						count += 1;
					});					
				}
				bool supportsVulkan1_3 = device.getProperties().apiVersion >= VK_API_VERSION_1_3;
				auto queueFamilies = device.getQueueFamilyProperties();
				bool supportsGraphics = std::ranges::any_of(queueFamilies,[](auto const& qfp){return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics);});
				auto availableDeviceExtensions = device.enumerateDeviceExtensionProperties();
				bool supportsAllRequiredExtensions = std::ranges::all_of(requiredDeviceExtension,[&availableDeviceExtensions](auto const&requiredDeviceExtension){return std::ranges::any_of(availableDeviceExtensions,[requiredDeviceExtension](auto const& availableDeviceExtension){return strcmp(availableDeviceExtension.extensionName, requiredDeviceExtension) == 0;});});
                auto features = device.template getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
                bool supportsRequiredFeatures = features.template get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
												features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
                                                features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;
                return supportsVulkan1_3 && supportsGraphics && supportsAllRequiredExtensions && supportsRequiredFeatures;
			});
			if (devIter != devices.end())
				physicalDevice = *devIter;
			else
				throw std::runtime_error("failed to find a suitable GPU!");
		}
		void CreateLogicalDevice(){
			std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
			auto graphicsQueueFamilyProperty = std::ranges::find_if(queueFamilyProperties,[](auto const&qfp){return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0);});
			assert(graphicsQueueFamilyProperty != queueFamilyProperties.end() && "No graphics queue family found!");
			queueIndex = static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), graphicsQueueFamilyProperty));
			if(queueIndex == ~0)
				throw std::runtime_error("Colud not find a queue for graphics and present -> terminating");

			//query vulakn
			vk::PhysicalDeviceVulkan11Features pv11;
			pv11.shaderDrawParameters = true;
			vk::PhysicalDeviceVulkan13Features pv13;
			pv13.dynamicRendering = true;
			vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT pded;
			pded.extendedDynamicState = true;
			vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT> featureChain =
			{
				{},
				pv11,
				pv13,
				pded
			};
			// create a Device
			float queuePriority = 0.5f;
			vk::DeviceQueueCreateInfo deviceQueueCreateInfo;
			deviceQueueCreateInfo.queueFamilyIndex = queueIndex;
			deviceQueueCreateInfo.queueCount = 1;
			deviceQueueCreateInfo.pQueuePriorities = &queuePriority;

			vk::DeviceCreateInfo deviceCreateInfo;
			deviceCreateInfo.queueCreateInfoCount = 1;
			deviceCreateInfo.pQueueCreateInfos = &deviceQueueCreateInfo;
			deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtension.size());
			deviceCreateInfo.ppEnabledExtensionNames = requiredDeviceExtension.data();
			deviceCreateInfo.pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>();

			device = vk::raii::Device(physicalDevice, deviceCreateInfo);
			queue = vk::raii::Queue(device, queueIndex, 0);
		}
		void CreateSwapChain(){
			auto surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(*surface);
			swapChainExtent = SelectSwapExtend(surfaceCapabilities);
			swapChainSurfaceFormat = SelectSwapSurfaceFormat(physicalDevice.getSurfaceFormatsKHR(*surface));
			vk::SwapchainCreateInfoKHR swapChainCreateInfo{};
			swapChainCreateInfo.surface = *surface;
			swapChainCreateInfo.minImageCount = SelectSwapMinImageCount(surfaceCapabilities);
			swapChainCreateInfo.imageFormat = swapChainSurfaceFormat.format;
			swapChainCreateInfo.imageColorSpace = swapChainSurfaceFormat.colorSpace;
			swapChainCreateInfo.imageExtent = swapChainExtent;
			swapChainCreateInfo.imageArrayLayers = 1;
			swapChainCreateInfo.imageUsage =vk::ImageUsageFlagBits::eColorAttachment;
			swapChainCreateInfo.imageSharingMode = vk::SharingMode::eExclusive;
			swapChainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
			swapChainCreateInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
			swapChainCreateInfo.presentMode = SelectSwapPresentMode(physicalDevice.getSurfacePresentModesKHR(*surface));
			swapChainCreateInfo.clipped = true;
			swapChain = vk::raii::SwapchainKHR(device, swapChainCreateInfo);
			swapChainImages = swapChain.getImages();
		}
		static vk::PresentModeKHR SelectSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes)
		{
			assert(std::ranges::any_of(availablePresentModes,[](auto presentMode){ return presentMode == vk::PresentModeKHR::eFifo;}));
			return std::ranges::any_of(availablePresentModes,[](const vk::PresentModeKHR value){
				return vk::PresentModeKHR::eMailbox == value;}) ? vk::PresentModeKHR::eMailbox : vk::PresentModeKHR::eFifo;
		}

		static uint32_t SelectSwapMinImageCount(vk::SurfaceCapabilitiesKHR const& surfaceCapabilities)
		{
			auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
			if((0 < surfaceCapabilities.maxImageCount) && (surfaceCapabilities.maxImageCount < minImageCount))
				minImageCount = surfaceCapabilities.maxImageCount;
			return minImageCount;
		}

		static vk::SurfaceFormatKHR SelectSwapSurfaceFormat(std::vector<vk::SurfaceFormatKHR> const& availableFormats)
		{
			assert(!availableFormats.empty());
			const auto formatIt = std::ranges::find_if(availableFormats,[](const auto& format){
				return format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
			});
			return formatIt != availableFormats.end() ? *formatIt : availableFormats[0];
		}
		vk::Extent2D SelectSwapExtend(const vk::SurfaceCapabilitiesKHR& capabilities){
			if(capabilities.currentExtent.width != 0xFFFFFFFF)
				return capabilities.currentExtent;
			int width, height;
			glfwGetFramebufferSize(window, &width, &height);
			return {
				std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
				std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)};
		}
		void CreateImageViews()
		{
			assert(swapChainImageViews.empty());
			vk::ImageViewCreateInfo imageViewCreateinfo{};
			imageViewCreateinfo.viewType = vk::ImageViewType::e2D;
			imageViewCreateinfo.format = swapChainSurfaceFormat.format;
			imageViewCreateinfo.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
			for(auto& image : swapChainImages)
			{
				imageViewCreateinfo.image = image;
				swapChainImageViews.emplace_back(device, imageViewCreateinfo);
			}
		}
		void CreateGraphicsPipeline()
		{
			vk::raii::ShaderModule shaderModule = CreateShaderModule(readFile("./slang.spv"));
			vk::PipelineShaderStageCreateInfo vertShaderStageInfo{};
			vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
			vertShaderStageInfo.module = shaderModule,
			vertShaderStageInfo.pName = "vertMain";
			
			vk::PipelineShaderStageCreateInfo fragShaderStageInfo{};
			fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
			fragShaderStageInfo.module = shaderModule;
			fragShaderStageInfo.pName = "fragMain";
			
			vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};
			//vertexMerge
			vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
			vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
			inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
			
			vk::PipelineViewportStateCreateInfo viewportState;
			viewportState.viewportCount = 1;
			viewportState.scissorCount = 1;

			vk::PipelineRasterizationStateCreateInfo rasterizer;
			rasterizer.depthClampEnable = vk::False;
			rasterizer.rasterizerDiscardEnable = vk::False;
			rasterizer.polygonMode = vk::PolygonMode::eFill;
			rasterizer.cullMode = vk::CullModeFlagBits::eBack;
			rasterizer.frontFace = vk::FrontFace::eClockwise;
			rasterizer.depthBiasEnable = vk::False;
			rasterizer.depthBiasSlopeFactor = 1.0f;
			rasterizer.lineWidth = 1.0f;

			vk::PipelineMultisampleStateCreateInfo multisampling;
			multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
			multisampling.sampleShadingEnable = vk::False;

			vk::PipelineColorBlendAttachmentState colorBlendAttachemnt;
			colorBlendAttachemnt.blendEnable = vk::False;
			colorBlendAttachemnt.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
			
			vk::PipelineColorBlendStateCreateInfo colorBlending;
			colorBlending.logicOpEnable = vk::False;
			colorBlending.logicOp = vk::LogicOp::eCopy;
			colorBlending.attachmentCount = 1;
			colorBlending.pAttachments = &colorBlendAttachemnt;

			std::vector dynamicStates={
				vk::DynamicState::eViewport,
				vk::DynamicState::eScissor
			};
			vk::PipelineDynamicStateCreateInfo dynamicState;
			dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
			dynamicState.pDynamicStates = dynamicStates.data();

			vk::PipelineLayoutCreateInfo pipeLineLayoutInfo;
			pipeLineLayout = vk::raii::PipelineLayout(device, pipeLineLayoutInfo);

			vk::GraphicsPipelineCreateInfo gpinfo;
			gpinfo.stageCount = 2;
			gpinfo.pStages = shaderStages;
			gpinfo.pVertexInputState = &vertexInputInfo;
			gpinfo.pInputAssemblyState = &inputAssembly;
			gpinfo.pViewportState = &viewportState;
			gpinfo.pRasterizationState = &rasterizer;
			gpinfo.pMultisampleState = &multisampling;
			gpinfo.pColorBlendState = &colorBlending;
			gpinfo.pDynamicState = &dynamicState;
			gpinfo.layout = pipeLineLayout;
			gpinfo.renderPass = nullptr;

			vk::PipelineRenderingCreateInfo pginfo;
			pginfo.colorAttachmentCount = 1;
			pginfo.pColorAttachmentFormats = &swapChainSurfaceFormat.format;

			vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain = {gpinfo, pginfo};
			
			grapicsPipeline = vk::raii::Pipeline(device, nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
		}
		//반환값을 강제
		[[nodiscard]] vk::raii::ShaderModule CreateShaderModule(const std::vector<char>& code) const
		{
			vk::ShaderModuleCreateInfo createInfo{};
			createInfo.codeSize = code.size() * sizeof(char);
			createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
			vk::raii::ShaderModule shaderModule{device, createInfo};
			return shaderModule;
		}
		void CreateCommandPool()
		{
			vk::CommandPoolCreateInfo poolInfo;
			poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
			poolInfo.queueFamilyIndex = queueIndex;
			commandPool = vk::raii::CommandPool(device, poolInfo);
		}
		void CreateCommandBuffer()
		{
			commandBuffers.clear();
			vk::CommandBufferAllocateInfo allocInfo;
			allocInfo.commandPool 	= commandPool;
			allocInfo.level			= vk::CommandBufferLevel::ePrimary;
			allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
			commandBuffers = vk::raii::CommandBuffers(device, allocInfo);
		}
		void RecordCommandBuffer(uint32_t imageIndex)
		{
			auto& commandBuffer = commandBuffers[frameIndex];
			commandBuffer.begin({});
			TranstionImageLayout(
									imageIndex,
									vk::ImageLayout::eUndefined,
									vk::ImageLayout::eColorAttachmentOptimal,
									{},
									vk::AccessFlagBits2::eColorAttachmentWrite,
									vk::PipelineStageFlagBits2::eColorAttachmentOutput,
									vk::PipelineStageFlagBits2::eColorAttachmentOutput);
			vk::ClearValue clearColor = vk::ClearColorValue(1.0f, 1.0f, 1.0f, 1.0f);
			vk::RenderingAttachmentInfo attachmentInfo;
			attachmentInfo.imageView	= swapChainImageViews[imageIndex];
			attachmentInfo.imageLayout	= vk::ImageLayout::eColorAttachmentOptimal;
			attachmentInfo.loadOp		= vk::AttachmentLoadOp::eClear;
			attachmentInfo.storeOp		= vk::AttachmentStoreOp::eStore;
			attachmentInfo.clearValue	= clearColor;
			vk::RenderingInfo renderingInfo;
			vk::Rect2D rect;
			rect.offset = vk::Offset2D{0, 0};
			rect.extent = swapChainExtent;
			renderingInfo.renderArea			= rect;
			renderingInfo.layerCount			= 1;
			renderingInfo.colorAttachmentCount 	= 1;
			renderingInfo.pColorAttachments		= &attachmentInfo;
			commandBuffer.beginRendering(renderingInfo);
			commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *grapicsPipeline);
			commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height), 0.0f, 1.0f));
			commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));
			commandBuffer.draw(3, 1, 0, 0);
			commandBuffer.endRendering();
			TranstionImageLayout(
									imageIndex,
									vk::ImageLayout::eColorAttachmentOptimal,
									vk::ImageLayout::ePresentSrcKHR,
									vk::AccessFlagBits2::eColorAttachmentWrite,
									{},
									vk::PipelineStageFlagBits2::eColorAttachmentOutput,
									vk::PipelineStageFlagBits2::eBottomOfPipe);
			commandBuffer.end();
		}
		void TranstionImageLayout(
									uint32_t				imageIndex,
									vk::ImageLayout			old_layout,
									vk::ImageLayout			new_layout,
									vk::AccessFlags2		src_access_mask,
									vk::AccessFlags2		dst_access_mask,
									vk::PipelineStageFlags2 src_stage_mask,
									vk::PipelineStageFlags2 dst_stage_mask)
		{
			vk::ImageMemoryBarrier2 barrier;
			barrier.srcStageMask			= src_stage_mask;
			barrier.srcAccessMask			= src_access_mask;
			barrier.dstStageMask			= dst_stage_mask;
			barrier.dstAccessMask			= dst_access_mask;
			barrier.oldLayout				= old_layout;
			barrier.newLayout				= new_layout;
			barrier.srcQueueFamilyIndex		= VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex		= VK_QUEUE_FAMILY_IGNORED;
			barrier.image					= swapChainImages[imageIndex];
			vk::ImageSubresourceRange subRange;
			subRange.aspectMask 	= vk::ImageAspectFlagBits::eColor;
			subRange.baseMipLevel	= 0;
			subRange.levelCount		= 1;
			subRange.baseArrayLayer = 0;
			subRange.layerCount		= 1;
			barrier.subresourceRange = subRange;
			vk::DependencyInfo dependencyInfo;
			dependencyInfo.dependencyFlags = {};
			dependencyInfo.imageMemoryBarrierCount = 1;
			dependencyInfo.pImageMemoryBarriers = &barrier;
			commandBuffers[frameIndex].pipelineBarrier2(dependencyInfo);
		}
		void CreateSyncObjects()
		{
			assert(presentCompleteSemaphores.empty() && renderFinishedSemaphores.empty() && inFlightFence.empty());
			for(size_t i = 0; i < swapChainImages.size(); i++)
				renderFinishedSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
			for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
			{
				presentCompleteSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
				vk::FenceCreateInfo fenceInfo;
				fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;
				inFlightFence.emplace_back(device, fenceInfo);
			}
		}


		static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type, const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData, void *)
		{
			if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError || severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
			{
				std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;
			}

			return vk::False;
		}

		static std::vector<char> readFile(const std::string &filename)
		{
			std::ifstream file(filename, std::ios::ate | std::ios::binary);
			if(!file.is_open())
				throw std::runtime_error("failed to open file!");
			std::vector<char> buffer(file.tellg());
			file.seekg(0, std::ios::beg);
			file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
			file.clear();
			return buffer;
		}

	private:
		GLFWwindow* 						window 			= nullptr;
		vk::raii::Context 					context;
		vk::raii::Instance 					instance 		= nullptr;
		vk::raii::DebugUtilsMessengerEXT 	debugMessenger 	= nullptr;
		vk::raii::SurfaceKHR 				surface 		= nullptr;
		//physical
		vk::raii::PhysicalDevice 			physicalDevice 	= nullptr;
		vk::raii::Device 					device 			= nullptr;
		//queue
		vk::raii::Queue 					queue	= nullptr;
		uint32_t queueIndex = ~0;
		//swapchain
		vk::raii::SwapchainKHR 				swapChain 		= nullptr;
		std::vector<vk::Image>				swapChainImages;
		vk::SurfaceFormatKHR				swapChainSurfaceFormat;
		vk::Extent2D						swapChainExtent;
		std::vector<vk::raii::ImageView>	swapChainImageViews;
		//grapics pipeline
		vk::raii::PipelineLayout 	pipeLineLayout = nullptr;
		vk::raii::Pipeline 			grapicsPipeline = nullptr;
		//conmmand
		vk::raii::CommandPool 					commandPool = nullptr;
		std::vector<vk::raii::CommandBuffer> 	commandBuffers;
		//sync object
		std::vector<vk::raii::Semaphore> 	presentCompleteSemaphores;
		std::vector<vk::raii::Semaphore> 	renderFinishedSemaphores;
		std::vector<vk::raii::Fence> 		inFlightFence;
		uint32_t							frameIndex = 0;

		bool framebufferResized = false;

		std::vector<const char*> requiredDeviceExtension = {
			vk::KHRSwapchainExtensionName};

};

int main()
{
	TriangleVulkan app;
	app.Run();
	return -1;
}