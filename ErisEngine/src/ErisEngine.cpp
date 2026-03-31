#include "ErisEngine.h"

glm::mat4 aiMatrix4x4ToGlm(const aiMatrix4x4& from) {
	glm::mat4 to;
	to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
	to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
	to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
	to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
	return to;
}

static PFN_vkVoidFunction Eris_ImGui_Loader(const char* function_name, void* user_data) {
    return vkGetInstanceProcAddr(*(VkInstance*)user_data, function_name);
}


VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr) {
		func(instance, debugMessenger, pAllocator);
	}
}


int ErisEngine::Eris_init() 
{
	if (!glfwInit()) {
		std::cerr << "Failed to initialize GLFW" << std::endl;
		return 0;
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	m_window = glfwCreateWindow(1280, 720, "Eris Engine", nullptr, nullptr);
	glfwSetWindowUserPointer(m_window, this);				// 绑定当前对象指针
	glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallback);

	if (!m_window) {
		std::cerr << "Failed to create GLFW window" << std::endl;
		return 0;
	}

	glfwMaximizeWindow(m_window);

	initVulkan();
	initVma();
	initSwapchain();
	createDepthBuffer();
	initRenderPass();
	initUIRenderPass();
	createFramebuffers();
	initSyncStructures();
	initCommands();
	initDescriptors();
	initDescriptorPool();
	initPipelines();
	initSkyboxPipeline();
	initGridPipeline();
	createSceneBuffers();
	initSkyboxMesh();

	m_activeWorld = new ErisWorld();
	m_activeWorld->getPhysics().createInfinitePlane();

	m_activeWorld->createDefaultSkybox();
	m_skyboxImage = loadCubemap(m_activeWorld->skyboxFaces);
	initSkyboxDescriptor();

	m_editor = new ErisEditor();
	m_editor->init(this);
	initViewportResources();
	// 如果没有加载出材质则用1x1白色代替
	initDefaultResources();
	// 初始化世界光
	initSceneData();


	Model* pSportCar = getOrLoadModel("assets/sportsCar/sportsCar.obj");
	Model* pGroundAsset = getOrLoadModel("assets/ground/churchfloor.obj");
	if (pGroundAsset) {
		RenderObject* ground = m_activeWorld->spawnObject(pGroundAsset, "Main_Ground");

		ground->m_location = glm::vec3(0.0f, -0.01f, 0.0f);

		ground->m_scale = glm::vec3(1.0f, 1.0f, 1.0f);

		ground->m_initialLocation = ground->m_location;
	}

	// debug用 测试对象
	{
		// 生成两辆车，放在不同位置
		RenderObject* car1 = m_activeWorld->spawnObject(pSportCar, "Main_Car");
		car1->m_location = glm::vec3(0.0f, 0.0f, 0.0f);

		RenderObject* car2 = m_activeWorld->spawnObject(pSportCar, "Backup_Car");
		car2->m_initialLocation=car2->m_location = glm::vec3(5.0f, 0.0f, -2.0f);
		car2->m_scale = glm::vec3(0.5f);

	}
	m_camera.m_position = glm::vec3(0.0f, 1.0f, 3.0f);



	m_isInitialized = true;
	return 1;

}

// 检查扩展是否支持交换链
bool ErisEngine:: checkDeviceExtensionSupport(VkPhysicalDevice device) {
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties>availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

	std::set<std::string>requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

	//debug test
	std::cout << "ExtensionNames Listed Below:\n";
	for (auto extensionName : requiredExtensions) {
		std::cout << "\t" << extensionName << "\n";
	}

	for (const auto& extension : availableExtensions) {
		requiredExtensions.erase(extension.extensionName);
	}

	return requiredExtensions.empty();
}

VkSurfaceFormatKHR ErisEngine::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
	// 判断首选组合是否可用
	for (const auto& availableFormat : availableFormats) {
		if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return availableFormat;
		}
	}

	return availableFormats[0];
}

VkPresentModeKHR ErisEngine::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
	for (const auto& availablePresentMode : availablePresentModes) {
		if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
			return availablePresentMode;
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D ErisEngine::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
	if (capabilities.currentExtent.width != (std::numeric_limits<uint32_t>::max)()) {
		return capabilities.currentExtent;
	}
	else {
		int width, height;
		glfwGetFramebufferSize(m_window, &width, &height);

		VkExtent2D actualExtent = {
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height)
		};

		actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

		return actualExtent;
	}
}

bool ErisEngine::loadShaderModule(const char* filePath, VkShaderModule* outShaderModule)
{
	std::ifstream file(filePath, std::ios::ate | std::ios::binary);
	if (!file.is_open()) return false;

	size_t fileSize = (size_t)file.tellg();
	std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));
	file.seekg(0);
	file.read((char*)buffer.data(), fileSize);
	file.close();

	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = buffer.size() * sizeof(uint32_t);
	createInfo.pCode = buffer.data();

	return vkCreateShaderModule(m_device, &createInfo, nullptr, outShaderModule) == VK_SUCCESS;
}

AllocatedImage ErisEngine::loadCubemap(const std::vector<std::string>& faces)
{
	if (faces.size() != 6) {
		throw std::runtime_error("Cubemap must have exactly 6 faces!");
	}

	int width, height, channels;
	std::vector<stbi_uc*> pixels(6);
	
	// 1. 加载 6 张图
	for (int i = 0; i < 6; i++) {
		pixels[i] = stbi_load(faces[i].c_str(), &width, &height, &channels, STBI_rgb_alpha);
		if (!pixels[i])throw std::runtime_error("failed to load cubemap face!");
	}

	VkDeviceSize layerSize = width * height * 4;
	VkDeviceSize totalSize = layerSize * 6;

	VkFormat imageFormat = VK_FORMAT_R8G8B8A8_SRGB; 
	VkExtent3D imageExtent;
	imageExtent.width = static_cast<uint32_t>(width);
	imageExtent.height = static_cast<uint32_t>(height);
	imageExtent.depth = 1;

	// 2. 创建 Staging Buffer
	AllocatedBuffer stagingBuffer = createBuffer(totalSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	void* data;
	vmaMapMemory(m_allocator, stagingBuffer.allocation, &data);
	for (int i = 0; i < 6; i++) {
		memcpy(static_cast<char*>(data) + (layerSize * i), pixels[i], layerSize);
		stbi_image_free(pixels[i]);
	}
	vmaUnmapMemory(m_allocator, stagingBuffer.allocation);

	// 3. 创建 GPU Image (关键：arrayLayers = 6, flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT)
	AllocatedImage newImage;
	VkImageCreateInfo imgInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	imgInfo.imageType = VK_IMAGE_TYPE_2D;
	imgInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
	imgInfo.extent = imageExtent;
	imgInfo.mipLevels = 1;
	imgInfo.arrayLayers = 6; // 6层
	imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imgInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT; // 标记为立方体贴图


	VmaAllocationCreateInfo vmaAllocInfo{ VMA_MEMORY_USAGE_GPU_ONLY };
	vmaCreateImage(m_allocator, &imgInfo, &vmaAllocInfo, &newImage.image, &newImage.allocation, nullptr);

	
	immediateSubmit([&](VkCommandBuffer cmd) {
		transitionImageLayout(cmd, newImage.image, imageFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,6);

		std::vector<VkBufferImageCopy>regions{};
		 
		for (uint32_t i = 0; i < 6; i++) {
			VkBufferImageCopy region{};
			region.bufferOffset = layerSize * i;
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.mipLevel = 0;
			region.imageSubresource.baseArrayLayer = i;
			region.imageSubresource.layerCount = 1;
			region.imageExtent = imageExtent;
			regions.push_back(region);
		}

		vkCmdCopyBufferToImage(cmd, stagingBuffer.buffer, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 6, regions.data());

		transitionImageLayout(cmd, newImage.image, imageFormat, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 6);
		});


	// 5. 创建特殊的 ImageView (viewType = VK_IMAGE_VIEW_TYPE_CUBE)
	VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	viewInfo.image = newImage.image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
	viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
	viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 };
	vkCreateImageView(m_device, &viewInfo, nullptr, &newImage.imageView);

	m_mainDeletionQueue.push_function([=]() {
		vkDestroyImageView(m_device, newImage.imageView, nullptr);
		vmaDestroyImage(m_allocator, newImage.image, newImage.allocation);
		});

	return newImage;
}

void ErisEngine::createSwapchain()
{
	SwapChainSupportDetails swapchainSupport = querySwapChainSupport(m_physicalDevice);

	VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapchainSupport.formats);
	VkPresentModeKHR presentMode = chooseSwapPresentMode(swapchainSupport.presentModes);
	VkExtent2D extent = chooseSwapExtent(swapchainSupport.capabilities);

	uint32_t imageCount = swapchainSupport.capabilities.minImageCount + 1;
	if (swapchainSupport.capabilities.maxImageCount > 0 && imageCount > swapchainSupport.capabilities.maxImageCount) {
		imageCount = swapchainSupport.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = m_surface;
	createInfo.minImageCount = imageCount;	// 几重缓冲
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	// imageArrayLayers 指定每个图像包含的层数。 除非您正在开发立体 3D 应用程序，否则此值始终为 1

	createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;		// 垂直同步
	createInfo.clipped = VK_TRUE;

	QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);
	uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(),indices.presentFamily.value() };

	if (indices.graphicsFamily != indices.presentFamily) {
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else {
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0;       //optional
		createInfo.pQueueFamilyIndices = nullptr;   //optional(可以不用写这两行)
	}

	createInfo.preTransform = swapchainSupport.capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;

	createInfo.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapchain) != VK_SUCCESS) {
		throw std::runtime_error("failed to create swap chain!");
	}

	vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, nullptr);
	m_swapchainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, m_swapchainImages.data());

	m_swapchainImageFormat = surfaceFormat.format;
	m_swapchainExtent = extent;
}

void ErisEngine::recreateSwapchain()
{
	int width = 0, height = 0;
	glfwGetFramebufferSize(m_window, &width, &height);

	while (width == 0 || height == 0) { // 处理最小化
		glfwGetFramebufferSize(m_window, &width, &height);
		glfwWaitEvents();
	}

	vkDeviceWaitIdle(m_device);

	cleanSwapchain();


	// 2. 重新创建
	createSwapchain();
	createImageViews();
	createDepthBuffer();    // 必须重新创建深度缓冲，因为它跟分辨率相关
	createFramebuffers();   // 重新绑定

	initViewportResources();
	ImGui_ImplVulkan_SetMinImageCount(static_cast<uint32_t>(m_swapchainImages.size()));
}

// 创建图像视图
void ErisEngine::createImageViews() {
	m_swapchainImageViews.resize(m_swapchainImages.size());

	for (size_t i = 0; i < m_swapchainImages.size(); i++) {
		m_swapchainImageViews[i] = createImageView(m_swapchainImages[i], m_swapchainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
	}

}

void ErisEngine::createFramebuffers()
{
	m_frameBuffers.resize(m_swapchainImageViews.size());
	for (size_t i = 0; i < m_swapchainImageViews.size(); i++) {
		std::array<VkImageView, 2> attachments = {
			//colorImageView,
			m_swapchainImageViews[i],
			m_depthImage.imageView
		};

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = m_renderPass;
		framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = m_swapchainExtent.width;
		framebufferInfo.height = m_swapchainExtent.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_frameBuffers[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create framebuffer!");
		}

	}
}

void ErisEngine::createSceneBuffers()
{
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		// 1. 创建显存中的缓冲区 (CPU_TO_GPU 模式方便每帧更新)
		m_frames[i].sceneBuffer = createBuffer(sizeof(GPUSceneData),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

		// 2. 分配 Set 0 (全局描述符)
		VkDescriptorSetAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		allocInfo.descriptorPool = m_descriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &m_globalSetLayout;
		vkAllocateDescriptorSets(m_device, &allocInfo, &m_frames[i].sceneDescriptorSet);

		// 3. 将 Buffer 写入（插进）描述符
		VkDescriptorBufferInfo binfo = {};
		binfo.buffer = m_frames[i].sceneBuffer.buffer;
		binfo.offset = 0;
		binfo.range = sizeof(GPUSceneData);

		VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		write.dstSet = m_frames[i].sceneDescriptorSet;
		write.dstBinding = 0;
		write.descriptorCount = 1;
		write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		write.pBufferInfo = &binfo;

		vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
	}


}

void ErisEngine::initSkyboxDescriptor()
{
	VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	allocInfo.descriptorPool = m_descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &m_skyboxDescriptorSetLayout; // 关键：使用天空盒布局

	vkAllocateDescriptorSets(m_device, &allocInfo, &m_skyboxDescriptorSet);

	VkDescriptorImageInfo imageInfo{};
	imageInfo.sampler = m_skyboxSampler;
	imageInfo.imageView = m_skyboxImage.imageView;
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkWriteDescriptorSet write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	write.dstSet = m_skyboxDescriptorSet;
	write.dstBinding = 0;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
}

void ErisEngine::initSkyboxMesh()
{
	std::vector<Vertex> skyboxVertices = {
		{{-1.0f,  1.0f, -1.0f}, {}, {}, {}},
		{{ 1.0f,  1.0f, -1.0f}, {}, {}, {}},
		{{ 1.0f, -1.0f, -1.0f}, {}, {}, {}},
		{{-1.0f, -1.0f, -1.0f}, {}, {}, {}},
		{{-1.0f,  1.0f,  1.0f}, {}, {}, {}},
		{{ 1.0f,  1.0f,  1.0f}, {}, {}, {}},
		{{ 1.0f, -1.0f,  1.0f}, {}, {}, {}},
		{{-1.0f, -1.0f,  1.0f}, {}, {}, {}} 
	};

	std::vector<uint32_t> skyboxIndices = {
		0, 1, 2, 2, 3, 0,
		4, 5, 6, 6, 7, 4,
		0, 4, 7, 7, 3, 0,
		1, 5, 6, 6, 2, 1,
		0, 1, 5, 5, 4, 0,
		3, 2, 6, 6, 7, 3
	};

	m_skyboxMesh.vertices = skyboxVertices;
	m_skyboxMesh.indices = skyboxIndices;
	m_skyboxMesh.uploadMesh(m_allocator, this);
}

VkImageView ErisEngine::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels)
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
	if (vkCreateImageView(m_device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
		throw std::runtime_error("failed to create image view!");
	}

	return imageView;
}

// 得到最多的多重采样
VkSampleCountFlagBits ErisEngine::getMaxUsableSampleCount() {
	VkPhysicalDeviceProperties physicalDeviceProperties;
	vkGetPhysicalDeviceProperties(m_physicalDevice, &physicalDeviceProperties);

	VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
	if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
	if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
	if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
	if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
	if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
	if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

	return VK_SAMPLE_COUNT_1_BIT;
}

void ErisEngine::initPipelines()
{
	VkShaderModule vertModule, fragModule;
	if (!loadShaderModule("shaders/vert.spv", &vertModule) || !loadShaderModule("shaders/frag.spv", &fragModule)) {
		throw std::runtime_error("failed to load shaders!");
	}

	std::array<VkDescriptorSetLayout, 2> layouts = { m_globalSetLayout,m_singleTextureLayout };

	// 管线布局（目前为空，以后放 MVP 矩阵）
	VkPushConstantRange pushConstantRange;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(MeshPushConstants);
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkPipelineLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.setLayoutCount = 2;
	layoutInfo.pSetLayouts = layouts.data();
	layoutInfo.pushConstantRangeCount = 1;
	layoutInfo.pPushConstantRanges = &pushConstantRange;

	if (vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create pipeline layout!");
	}

	PipelineBuilder builder;
	// 1. 添加着色器阶段
	builder.m_shaderStages.push_back({ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertModule, "main" });
	builder.m_shaderStages.push_back({ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragModule, "main" });


	// 2. 设置顶点输入（现在是空的）
	builder.m_vertexBindings.push_back(Vertex::getBindingDescription());
	builder.m_vertexAttributes = Vertex::getAttributeDescriptions();

	builder.m_vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	builder.m_vertexInputInfo.pNext = nullptr;

	// 3. 设置拓扑（三角形）
	builder.m_inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	builder.m_inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	builder.m_inputAssembly.primitiveRestartEnable = VK_FALSE;

	// 4. 设置视口
	builder.m_dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	builder.m_viewport = { 0.0f,0.0f,(float)m_swapchainExtent.width,(float)m_swapchainExtent.height,0.0f,1.0f };
	builder.m_scissor = { {0,0},m_swapchainExtent };

	// 5. 光栅化
	builder.m_rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	builder.m_rasterizer.pNext = nullptr;
	builder.m_rasterizer.flags = 0;
	builder.m_rasterizer.depthClampEnable = VK_FALSE;
	builder.m_rasterizer.rasterizerDiscardEnable = VK_FALSE;
	builder.m_rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	//builder.m_rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	builder.m_rasterizer.cullMode = VK_CULL_MODE_NONE;
	builder.m_rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;	// vulkan翻转Y轴了
	builder.m_rasterizer.depthBiasEnable = VK_FALSE;
	builder.m_rasterizer.depthBiasConstantFactor = 0.0f;
	builder.m_rasterizer.depthBiasClamp = 0.0f;
	builder.m_rasterizer.depthBiasSlopeFactor = 0.0f;
	builder.m_rasterizer.lineWidth = 1.0f;

	// 6. 混合模式（不透明）
	builder.m_colorBlendAttachment.blendEnable = VK_FALSE;
	builder.m_colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	builder.m_colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	builder.m_colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	builder.m_colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	builder.m_colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	builder.m_colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
	builder.m_colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	// --- 必须增加：深度测试配置 ---
	builder.m_depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	builder.m_depthStencil.pNext = nullptr;
	builder.m_depthStencil.depthTestEnable = VK_TRUE;           // 开启深度测试
	builder.m_depthStencil.depthWriteEnable = VK_TRUE;          // 开启深度写入
	builder.m_depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL; // 越近的物体越先画
	builder.m_depthStencil.depthBoundsTestEnable = VK_FALSE;
	builder.m_depthStencil.minDepthBounds = 0.0f;
	builder.m_depthStencil.maxDepthBounds = 1.0f;
	builder.m_depthStencil.stencilTestEnable = VK_FALSE;        // 暂时不启用模板测试

	// 7. 多重采样
	builder.m_multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	builder.m_multisampling.sampleShadingEnable = VK_FALSE;
	builder.m_multisampling.rasterizationSamples = msaaSamples;

	builder.m_pipelineLayout = m_pipelineLayout;
	m_pipeline = builder.buildPipeline(m_device, m_renderPass);

	
	vkDestroyShaderModule(m_device, vertModule, nullptr);
	vkDestroyShaderModule(m_device, fragModule, nullptr);

	m_mainDeletionQueue.push_function([=]() {
		vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
		vkDestroyPipeline(m_device, m_pipeline, nullptr);
		});
}

void ErisEngine::initSkyboxPipeline()
{
	VkShaderModule skyVert, skyFrag;
	if (!loadShaderModule("shaders/sky_vert.spv", &skyVert) || !loadShaderModule("shaders/sky_frag.spv", &skyFrag)) {
		throw std::runtime_error("failed to load skybox shaders!");
	}

	std::array<VkDescriptorSetLayout, 2> skyboxLayouts = { m_globalSetLayout, m_skyboxDescriptorSetLayout };
	VkPipelineLayoutCreateInfo skyLayoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	skyLayoutInfo.setLayoutCount = 2;
	skyLayoutInfo.pSetLayouts = skyboxLayouts.data();
	// 天空盒通常不需要 PushConstants，或者只需要一个缩放系数
	skyLayoutInfo.pushConstantRangeCount = 0;

	if (vkCreatePipelineLayout(m_device, &skyLayoutInfo, nullptr, &m_skyboxPipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create skybox pipeline layout!");
	}

	PipelineBuilder skyBuilder;

	VkPipelineShaderStageCreateInfo vertStage = {};
	vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertStage.module = skyVert;
	vertStage.pName = "main";

	VkPipelineShaderStageCreateInfo fragStage = {};
	fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragStage.module = skyFrag;
	fragStage.pName = "main";

	skyBuilder.m_shaderStages.push_back(vertStage);
	skyBuilder.m_shaderStages.push_back(fragStage);

	skyBuilder.m_vertexBindings.push_back(Vertex::getBindingDescription());
	skyBuilder.m_vertexAttributes = Vertex::getAttributeDescriptions();
	skyBuilder.m_vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	skyBuilder.m_inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	skyBuilder.m_inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	skyBuilder.m_inputAssembly.primitiveRestartEnable = VK_FALSE;

	skyBuilder.m_viewport = { 0.0f, 0.0f, (float)m_swapchainExtent.width, (float)m_swapchainExtent.height, 0.0f, 1.0f };
	skyBuilder.m_scissor = { {0, 0}, m_swapchainExtent };

	skyBuilder.m_rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	skyBuilder.m_rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	skyBuilder.m_rasterizer.cullMode = VK_CULL_MODE_NONE;
	//skyBuilder.m_rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	skyBuilder.m_rasterizer.lineWidth = 1.0f;

	skyBuilder.m_multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	skyBuilder.m_multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	skyBuilder.m_colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	skyBuilder.m_colorBlendAttachment.blendEnable = VK_FALSE;

	skyBuilder.m_depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	skyBuilder.m_depthStencil.depthTestEnable = VK_TRUE;
	skyBuilder.m_depthStencil.depthWriteEnable = VK_FALSE; // 关键：天空盒不写入深度，不遮挡后续UI
	skyBuilder.m_depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

	skyBuilder.m_pipelineLayout = m_skyboxPipelineLayout;

	m_skyboxPipeline = skyBuilder.buildPipeline(m_device, m_renderPass);
	vkDestroyShaderModule(m_device, skyVert, nullptr);
	vkDestroyShaderModule(m_device, skyFrag, nullptr);

	m_mainDeletionQueue.push_function([=]() {
		vkDestroyPipelineLayout(m_device, m_skyboxPipelineLayout, nullptr);
		vkDestroyPipeline(m_device, m_skyboxPipeline, nullptr);
		});
}

void ErisEngine::initGridPipeline()
{
	VkShaderModule vertMod, fragMod;
	if (!loadShaderModule("shaders/grid_vert.spv", &vertMod) != VK_SUCCESS
		|| !loadShaderModule("shaders/grid_frag.spv", &fragMod) != VK_SUCCESS) {
		throw std::runtime_error("failed to load grid shaders!");
	}

	std::array<VkDescriptorSetLayout, 1>layouts = { m_globalSetLayout };
	VkPipelineLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	layoutInfo.setLayoutCount = 1;
	layoutInfo.pSetLayouts = layouts.data();
	if (vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_gridPipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create grid pipeline layout!");
	}
	PipelineBuilder builder;
	builder.m_shaderStages.push_back({ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertMod, "main" });
	builder.m_shaderStages.push_back({ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragMod, "main" });

	builder.m_vertexInputInfo = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	builder.m_inputAssembly = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE };

	builder.m_viewport = { 0.0f, 0.0f, (float)m_swapchainExtent.width, (float)m_swapchainExtent.height, 0.0f, 1.0f };
	builder.m_scissor = { {0, 0}, m_swapchainExtent };
	builder.m_rasterizer = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, nullptr, 0, VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE, VK_FALSE, 0.0f, 0.0f, 0.0f, 1.0f };
	builder.m_multisampling = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, nullptr, 0, VK_SAMPLE_COUNT_1_BIT };

	builder.m_colorBlendAttachment.blendEnable = VK_TRUE;
	builder.m_colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	builder.m_colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	builder.m_colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	builder.m_colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	builder.m_colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	builder.m_colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
	builder.m_colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	builder.m_depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	builder.m_depthStencil.depthTestEnable = VK_TRUE;  // 必须开启，用于检测是否被车遮挡
	builder.m_depthStencil.depthWriteEnable = VK_FALSE; // 必须关闭，因为网格是透明的
	builder.m_depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

	builder.m_pipelineLayout = m_gridPipelineLayout;
	m_gridPipeline = builder.buildPipeline(m_device, m_renderPass);

	vkDestroyShaderModule(m_device, vertMod, nullptr);
	vkDestroyShaderModule(m_device, fragMod, nullptr);

	m_mainDeletionQueue.push_function([=]() {
		vkDestroyPipelineLayout(m_device, m_gridPipelineLayout, nullptr);
		vkDestroyPipeline(m_device, m_gridPipeline, nullptr);
		});

}

FrameData& ErisEngine::getCurrentFrame()
{
	return m_frames[m_frameNumber % MAX_FRAMES_IN_FLIGHT];
}


void ErisEngine::Eris_run()
{
	while (!glfwWindowShouldClose(m_window))
	{
		glfwPollEvents();
		drawFrame();
	}
}


void ErisEngine::Eris_cleanup()
{
	if (m_isInitialized) {
		vkDeviceWaitIdle(m_device);

		if (m_editor) {
			m_editor->cleanup(m_device);
			vkDestroyDescriptorPool(m_device, m_imguiPool, nullptr);
			delete m_editor;
		}

		cleanSwapchain();

		m_mainDeletionQueue.flush();

		if (m_debugMessenger != VK_NULL_HANDLE) {
			DestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
		}
		vkDestroyDevice(m_device, nullptr);
		vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
		vkDestroyInstance(m_instance, nullptr);

		glfwDestroyWindow(m_window);
		glfwTerminate();
	}
}

void ErisEngine:: cleanSwapchain()
{
	// 显式销毁帧缓冲
	if (m_viewportFramebuffer != VK_NULL_HANDLE) {
		vkDestroyFramebuffer(m_device, m_viewportFramebuffer, nullptr);
		m_viewportFramebuffer = VK_NULL_HANDLE;
	}
	for (auto fb : m_frameBuffers) {
		vkDestroyFramebuffer(m_device, fb, nullptr);
	}
	m_frameBuffers.clear();

	// 销毁视口贴图资源
	if (m_viewportImage.imageView != VK_NULL_HANDLE) {
		vkDestroyImageView(m_device, m_viewportImage.imageView, nullptr);
		m_viewportImage.imageView = VK_NULL_HANDLE;
	}
	if (m_viewportImage.image != VK_NULL_HANDLE) {
		vmaDestroyImage(m_allocator, m_viewportImage.image, m_viewportImage.allocation);
		m_viewportImage.image = VK_NULL_HANDLE;
	}


	// 显式销毁深度资源
	if (m_depthImage.imageView != VK_NULL_HANDLE) {
		vkDestroyImageView(m_device, m_depthImage.imageView, nullptr);
		m_depthImage.imageView = VK_NULL_HANDLE;
	}
	if (m_depthImage.image != VK_NULL_HANDLE) {
		vmaDestroyImage(m_allocator, m_depthImage.image, m_depthImage.allocation);
		m_depthImage.image = VK_NULL_HANDLE;
	}

	// 销毁旧的图像视图
	for (auto view : m_swapchainImageViews) {
		vkDestroyImageView(m_device, view, nullptr);
	}
	m_swapchainImageViews.clear();

	if (m_swapchain != VK_NULL_HANDLE) {
		vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
		m_swapchain = VK_NULL_HANDLE;
	}
}

void ErisEngine::initVulkan()
{
	// 
    // 1. 创建 Instance
	createInstance();

	// 2. 创建 Surface
	createSurface();

	// 3. 挑选物理设备 (GPU)
	pickPhysicalDevice();

	// 4. 创建逻辑设备和获取队列
	// 查找图形队列族
	createLogicalDevice();


}


void ErisEngine::initVma()
{
	// 1. 显式加载 Vulkan 函数指针
	VmaVulkanFunctions vulkanFunctions = {};
	vulkanFunctions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
	vulkanFunctions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;
	// 如果你使用的是 Vulkan 1.1+，建议补全这些（可选但推荐）
	vulkanFunctions.vkGetPhysicalDeviceMemoryProperties = &vkGetPhysicalDeviceMemoryProperties;
	vulkanFunctions.vkGetDeviceBufferMemoryRequirements = &vkGetDeviceBufferMemoryRequirements;
	vulkanFunctions.vkGetDeviceImageMemoryRequirements = &vkGetDeviceImageMemoryRequirements;

	VmaAllocatorCreateInfo allocatorInfo{};
	allocatorInfo.physicalDevice = m_physicalDevice;
	allocatorInfo.device = m_device;
	allocatorInfo.instance = m_instance;
	allocatorInfo.pVulkanFunctions = &vulkanFunctions; // 核心：传入函数指针表

	// 对应你 Instance 里的 apiVersion
	allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_0;

	if (vmaCreateAllocator(&allocatorInfo, &m_allocator) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create VMA Allocator!");
	}

	m_mainDeletionQueue.push_function([=]() {
		vmaDestroyAllocator(m_allocator);
		});
}

void ErisEngine::initSwapchain()
{
	// 此处应包含创建 VkSwapchainKHR 的代码
	// 赋值给 m_swapchain, m_swapchainImageFormat, m_swapchainImages, m_swapchainImageViews

	createSwapchain();
	createImageViews();
}

void ErisEngine::initCommands()
{
	// 此处应包含创建 VkCommandPool 的逻辑
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		// 创建每帧的命令池
		VkCommandPoolCreateInfo commandPoolInfo{};
		commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		commandPoolInfo.queueFamilyIndex = m_graphicsQueueFamily;
		commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		if (vkCreateCommandPool(m_device, &commandPoolInfo, nullptr, &m_frames[i].m_commandPool) != VK_SUCCESS) {
			throw std::runtime_error("failed to create per-frame command pool!");
		}

		// 分配每帧的主命令缓冲
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = m_frames[i].m_commandPool;
		allocInfo.commandBufferCount = 1;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		vkAllocateCommandBuffers(m_device, &allocInfo, &m_frames[i].m_mainCommandBuffer);

		// 注册销毁
		m_mainDeletionQueue.push_function([=]() {
			vkDestroyCommandPool(m_device, m_frames[i].m_commandPool, nullptr);
			});
	}

}



void ErisEngine::initSyncStructures()
{
	// 此处应包含创建 VkFence 和 VkSemaphore 的逻辑

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	// 初始状态设为 Signaled，确保第一帧不会无限等待
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		if (vkCreateFence(m_device, &fenceInfo, nullptr, &m_frames[i].m_renderFence) != VK_SUCCESS ||
			vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_frames[i].m_presentSemaphore) != VK_SUCCESS ||
			vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_frames[i].m_renderSemaphore) != VK_SUCCESS) {
			throw std::runtime_error("failed to create synchronization structures for a frame!");
		}

		m_mainDeletionQueue.push_function([=]() {
			vkDestroyFence(m_device, m_frames[i].m_renderFence, nullptr);
			vkDestroySemaphore(m_device, m_frames[i].m_presentSemaphore, nullptr);
			vkDestroySemaphore(m_device, m_frames[i].m_renderSemaphore, nullptr);
			});
	}

}

void ErisEngine::initRenderPass()
{
	// 1. 颜色附件描述
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = m_swapchainImageFormat;
	colorAttachment.samples = msaaSamples;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;  // 每一帧开始时：清除
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // 每一帧结束时：保存渲染结果
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // 保持在渲染目标状态

	// 2. 深度附件 (新增)
	VkAttachmentDescription depthAttachment{};
	depthAttachment.format = m_depthFormat;
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;


	// 2. 附件引用
	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0; // 对应 pAttachments 数组中的索引
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef{};
	depthAttachmentRef.attachment = 1; // 索引为 1
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// 3. 子通道 (Subpass)
	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;

	// 4. 子通道依赖 (防止在图像还没准备好时就开始清除操作)
	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;



	// 5. 创建 Render Pass
	std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS) {
		throw std::runtime_error("failed to create render pass!");
	}

	// 注册销毁
	m_mainDeletionQueue.push_function([=]() {
		vkDestroyRenderPass(m_device, m_renderPass, nullptr);
		});
}


void ErisEngine::initUIRenderPass() {
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = m_swapchainImageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // UI 层也清除一次
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // 最终给显示器

	// 2. 深度附件 (Attachment 1) - 即使 UI 不用，也必须声明以保持兼容
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = m_depthFormat; // 确保使用和你主 Pass 一样的深度格式
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;  // UI 不关心深度内容
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;


	VkAttachmentReference colorRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	VkAttachmentReference depthRef = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorRef;
	subpass.pDepthStencilAttachment = &depthRef;


	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	
	
	std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
	VkRenderPassCreateInfo createInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	createInfo.attachmentCount = 2;
	createInfo.pAttachments = attachments.data();
	createInfo.subpassCount = 1;
	createInfo.pSubpasses = &subpass;
	createInfo.dependencyCount = 1;
	createInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(m_device, &createInfo, nullptr, &m_uiRenderPass) != VK_SUCCESS) {
		throw std::runtime_error("failed to create UI render pass!");
	}

	m_mainDeletionQueue.push_function([=]() {
		vkDestroyRenderPass(m_device, m_uiRenderPass, nullptr);
		});
}

bool ErisEngine:: checkValidationLayerSupport() 
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const char* layerName : validationLayers) {
		bool layerFound = false;

		for (const auto& layerProperties : availableLayers) {
			if (strcmp(layerName, layerProperties.layerName) == 0) {
				layerFound = true;
				break;
			}
		}

		if (!layerFound) {
			return false;
		}
	}

	return true;
}

SwapChainSupportDetails ErisEngine::querySwapChainSupport(VkPhysicalDevice device) {
	SwapChainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);

	if (formatCount != 0) {
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, details.formats.data());
	}

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, nullptr);

	if (presentModeCount != 0) {
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, details.presentModes.data());
	}

	return details;
}



void ErisEngine::createDepthBuffer()
{ 
	// 1. 确定格式和尺寸
	m_depthFormat = findDepthFormat();

	VkExtent3D depthImageExtent = {
		m_swapchainExtent.width,
		m_swapchainExtent.height,
		1
	};

	// 2. 配置图像信息 (VkImageCreateInfo)
	VkImageCreateInfo depthImageInfo{};
	depthImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	depthImageInfo.imageType = VK_IMAGE_TYPE_2D;
	depthImageInfo.format = m_depthFormat;
	depthImageInfo.extent = depthImageExtent;
	depthImageInfo.mipLevels = 1;
	depthImageInfo.arrayLayers = 1;
	depthImageInfo.samples = msaaSamples;
	depthImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	depthImageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT; // 关键：指定为深度附件

	// 3. 配置 VMA 内存分配信息 (VmaAllocationCreateInfo)
	// 这取代了教程中的 VkMemoryAllocateInfo
	VmaAllocationCreateInfo depthAllocInfo{};	
	depthAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;	// 告诉 VMA 这块内存在显存中
	depthAllocInfo.requiredFlags = (VkMemoryPropertyFlags)VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	// 4. 使用 VMA 一次性创建 Image 和 Allocation
   // 这取代了教程中的 vkCreateImage 和 vkAllocateMemory
	VkResult res = vmaCreateImage(m_allocator, &depthImageInfo, &depthAllocInfo,
		&m_depthImage.image,
		&m_depthImage.allocation,
		nullptr);

	if (res != VK_SUCCESS) {
		throw std::runtime_error("failed to create depth image! Result: " + std::to_string(res));
	}

	// 5. 配置并创建图像视图 (VkImageViewCreateInfo)
	VkImageViewCreateInfo depthImageViewInfo{};
	depthImageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	depthImageViewInfo.pNext = nullptr;
	depthImageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	depthImageViewInfo.image = m_depthImage.image;
	depthImageViewInfo.format = m_depthFormat;
	depthImageViewInfo.subresourceRange.baseMipLevel = 0;
	depthImageViewInfo.subresourceRange.levelCount = 1;
	depthImageViewInfo.subresourceRange.baseArrayLayer = 0;
	depthImageViewInfo.subresourceRange.layerCount = 1;
	depthImageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT; // 关键：标记为深度视角

	if (vkCreateImageView(m_device, &depthImageViewInfo, nullptr, &m_depthImage.imageView) != VK_SUCCESS) {
		throw std::runtime_error("failed to create depth image view!");
	}

}

VkFormat ErisEngine::findDepthFormat() {
	return findSupportedFormat(
		{ VK_FORMAT_D32_SFLOAT,VK_FORMAT_D32_SFLOAT_S8_UINT,VK_FORMAT_D24_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
	);
}

VkFormat ErisEngine::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
	for (VkFormat format : candidates) {
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(m_physicalDevice, format, &props);
		if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
			return format;
		}
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
			return format;
		}
	}

	throw std::runtime_error("failed to find supported format!");
}

bool ErisEngine::isDeviceSuitable(VkPhysicalDevice device) {
	QueueFamilyIndices indices = findQueueFamilies(device);

	bool extensionsSupported = checkDeviceExtensionSupport(device);

	bool swapChainAdequate = false;
	if (extensionsSupported) {
		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
		swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
	}

	VkPhysicalDeviceFeatures supportedFeatures;
	vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

	return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}

// 初始化验证层信息
void ErisEngine::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
	createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = debugCallback;
}


void ErisEngine::setupDebugMessenger()
{
	if (!enableValidationLayers)return;

	VkDebugUtilsMessengerCreateInfoEXT createInfo;
	populateDebugMessengerCreateInfo(createInfo);

	if (CreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_debugMessenger) != VK_SUCCESS) {
		throw std::runtime_error("failed to set up debug messenger!\n");
	}

}

void ErisEngine::createSurface()
{
	if (glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface) != VK_SUCCESS) {
		throw std::runtime_error("failed to create window surface!");
	}
}

void ErisEngine::createLogicalDevice()
{
	QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<uint32_t>uniqueQueueFamilies = {
		indices.graphicsFamily.value(),
		indices.presentFamily.value() };

	float queuePriority = 1.0f;
	for (uint32_t queueFamily : uniqueQueueFamilies) {
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkPhysicalDeviceFeatures deviceFeatures{};
	deviceFeatures.samplerAnisotropy = VK_TRUE;

	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	createInfo.pQueueCreateInfos = queueCreateInfos.data();

	createInfo.pEnabledFeatures = &deviceFeatures;

	createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	createInfo.ppEnabledExtensionNames = deviceExtensions.data();

	if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS) {
		throw std::runtime_error("failed to create logical device!");
	}

	vkGetDeviceQueue(m_device, indices.graphicsFamily.value(), 0, &m_graphicsQueue);
	vkGetDeviceQueue(m_device, indices.presentFamily.value(), 0, &m_presentQueue);
}

// 查找支持图形命令的队列
QueueFamilyIndices ErisEngine::findQueueFamilies(VkPhysicalDevice device) {
	QueueFamilyIndices indices;

	// Logic to find queue family indices to populate struct with

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties>queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	int i = 0;
	for (const auto& queueFamily : queueFamilies) {
		if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			indices.graphicsFamily = i;
		}
		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);

		if (presentSupport) {
			indices.presentFamily = i;
		}

		if (indices.isComplete()) {
			break;
		}

		i++;
	}

	return indices;
}

std::vector<const char*> ErisEngine::getRequiredExtensions() {
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	std::vector<const char*>extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
	if (enableValidationLayers) {
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}
	return extensions;
}

void ErisEngine::pickPhysicalDevice()
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
	if (deviceCount == 0) {
		throw std::runtime_error("failed to find GPUs with Vulkan support!");
	}

	std::vector<VkPhysicalDevice>devices(deviceCount);
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

	for (const auto& device : devices) {
		if (isDeviceSuitable(device)) {
			m_physicalDevice = device;
			// msaaSamples = getMaxUsableSampleCount();
			msaaSamples = VK_SAMPLE_COUNT_1_BIT;
			break;
		}
	}

	if (m_physicalDevice == VK_NULL_HANDLE) {
		throw std::runtime_error("failed to find a suitable GPU!");
	}
}

void ErisEngine::createInstance()
{
	if (enableValidationLayers && !checkValidationLayerSupport()) {
		throw std::runtime_error("validation layers requested, but not available!\n");
	}
	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Eris Engine";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "Eris";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;


	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	auto extensions = getRequiredExtensions();
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();

	// 调试信使
	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
	if (enableValidationLayers) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();

		populateDebugMessengerCreateInfo(debugCreateInfo);
		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
	}
	else {
		createInfo.enabledExtensionCount = 0;

		createInfo.pNext = nullptr;
	}


	if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS) {
		throw std::runtime_error("failed to create instance!");
	}
	else {
		std::cout << "created instance successfully!" << std::endl;
	}
}


AllocatedBuffer ErisEngine::createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage) 
{
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.pNext = nullptr;
	bufferInfo.size = allocSize;
	bufferInfo.usage = usage;

	VmaAllocationCreateInfo vmaAllocInfo{};
	vmaAllocInfo.usage = memoryUsage;

	AllocatedBuffer buffer;

	if (vmaCreateBuffer(m_allocator, &bufferInfo, &vmaAllocInfo,
		&buffer.buffer,
		&buffer.allocation,
		nullptr) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create Vulkan Buffer via VMA!");
	}

	m_mainDeletionQueue.push_function([=]() {
		vmaDestroyBuffer(m_allocator, buffer.buffer, buffer.allocation);
		});

	return buffer;

}

void ErisEngine::immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function) 
{
	// 1. 分配临时的命令缓冲
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = m_frames[0].m_commandPool; // 借用第0帧的池
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer cmd;
	vkAllocateCommandBuffers(m_device, &allocInfo, &cmd);

	// 2. 开始录制
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(cmd, &beginInfo);

	// 3. 执行外部传入的 Lambda 逻辑（比如拷贝缓冲）
	function(cmd);

	// 4. 结束并提交
	vkEndCommandBuffer(cmd);

	VkSubmitInfo submit{};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &cmd;

	vkQueueSubmit(m_graphicsQueue, 1, &submit, VK_NULL_HANDLE);
	vkQueueWaitIdle(m_graphicsQueue); // 等待拷贝完成

	// 5. 释放临时缓冲
	vkFreeCommandBuffers(m_device, m_frames[0].m_commandPool, 1, &cmd);
}

void ErisEngine::initDescriptors() {
	// ---------------------------------------------------------
	// 1. 创建全局布局 (Set 0) - 用于 GPUSceneData
	// ---------------------------------------------------------
	VkDescriptorSetLayoutBinding sceneBind{};
	sceneBind.binding = 0;
	sceneBind.descriptorCount = 1;
	sceneBind.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	sceneBind.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo globalLayoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	globalLayoutInfo.bindingCount = 1;
	globalLayoutInfo.pBindings = &sceneBind;
	if (vkCreateDescriptorSetLayout(m_device, &globalLayoutInfo, nullptr, &m_globalSetLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create global descriptor set layout!");
	}

	// ---------------------------------------------------------
	// 2.1 创建材质布局 (Set 1) - 用于贴图
	// ---------------------------------------------------------

	VkDescriptorSetLayoutBinding textureBind{};
	textureBind.binding = 0;
	textureBind.descriptorCount = 1;
	textureBind.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	textureBind.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &textureBind;
	if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_singleTextureLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create singletexture descriptor set layout!");
	}

	// ---------------------------------------------------------
	// 2.2 创建天空盒布局 (Set 1) - 用于贴图
	// ---------------------------------------------------------

	VkDescriptorSetLayoutBinding skyboxBind{};
	skyboxBind.binding = 0;
	skyboxBind.descriptorCount = 1;
	skyboxBind.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	skyboxBind.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo skyInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	skyInfo.bindingCount = 1;
	skyInfo.pBindings = &skyboxBind; 
	if (vkCreateDescriptorSetLayout(m_device, &skyInfo, nullptr, &m_skyboxDescriptorSetLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create skybox descriptor set layout!");
	}

	// 3. 创建采样器 (Sampler)
	VkSamplerCreateInfo samplerInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	if (vkCreateSampler(m_device, &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS) {
		throw std::runtime_error("failed to create original sampler!");
	};

	VkSamplerCreateInfo skySamplerInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	skySamplerInfo.magFilter = VK_FILTER_LINEAR;
	skySamplerInfo.minFilter = VK_FILTER_LINEAR;
	// 关键：防止边缘接缝
	skySamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	skySamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	skySamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	if (vkCreateSampler(m_device, &skySamplerInfo, nullptr, &m_skyboxSampler) != VK_SUCCESS) {
		throw std::runtime_error("failed to create skybox sampler!");
	}

	// 注册销毁 (注意顺序：先删 Pool 再删 Layout)
	m_mainDeletionQueue.push_function([=]() {
		vkDestroySampler(m_device, m_sampler, nullptr);
		vkDestroySampler(m_device, m_skyboxSampler, nullptr);
		vkDestroyDescriptorSetLayout(m_device, m_globalSetLayout, nullptr);
		vkDestroyDescriptorSetLayout(m_device, m_singleTextureLayout, nullptr);
		vkDestroyDescriptorSetLayout(m_device, m_skyboxDescriptorSetLayout, nullptr);
		});
}

void ErisEngine::initDescriptorPool()
{
	std::vector<VkDescriptorPoolSize>sizes = {
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,100},			// 支持 100 个全局数据块
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,500}		// 支持 500 张材质贴图
	};

	VkDescriptorPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;						// 允许单独释放
	poolInfo.maxSets = 600;																	// 总集数
	poolInfo.poolSizeCount = static_cast<uint32_t>(sizes.size());
	poolInfo.pPoolSizes = sizes.data();

	if (vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor pool!");
	}

	m_mainDeletionQueue.push_function([=]() {
		vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
		});
}

VkDescriptorSet ErisEngine::createDescriptorSet(AllocatedImage& img) {
	if (m_descriptorPool == VK_NULL_HANDLE || m_singleTextureLayout == VK_NULL_HANDLE) {
		std::cerr << "Descriptor resources not initialized!" << std::endl;
		return VK_NULL_HANDLE;
	}

	VkDescriptorSet set;
	VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	allocInfo.descriptorPool = m_descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &m_singleTextureLayout;

	if (vkAllocateDescriptorSets(m_device, &allocInfo, &set) != VK_SUCCESS) {
		return VK_NULL_HANDLE;
	}

	VkDescriptorImageInfo imageBufferInfo{};
	imageBufferInfo.sampler = m_sampler; // 确保你已经在 initDescriptors 里创建了它
	imageBufferInfo.imageView = img.imageView;
	imageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkWriteDescriptorSet textureWrite{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	textureWrite.dstSet = set;
	textureWrite.dstBinding = 0;
	textureWrite.descriptorCount = 1;
	textureWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	textureWrite.pImageInfo = &imageBufferInfo;

	vkUpdateDescriptorSets(m_device, 1, &textureWrite, 0, nullptr);
	return set;
}

VKAPI_ATTR VkBool32 VKAPI_CALL ErisEngine::debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData
) {
	std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
	return VK_FALSE;
}

void ErisEngine::drawFrame()
{
	
	static auto lastTime = std::chrono::high_resolution_clock::now();
	auto currentTime= std::chrono::high_resolution_clock::now();
	float deltaTime= std::chrono::duration<float, std::chrono::seconds::period>(currentTime - lastTime).count();
	lastTime = currentTime;

	if (m_activeWorld) {
		m_activeWorld->update(deltaTime);
	}

	handleInput(deltaTime);

	FrameData& frame = getCurrentFrame();
	vkWaitForFences(m_device, 1, &frame.m_renderFence, VK_TRUE, UINT64_MAX);

	VkCommandBuffer cmd = frame.m_mainCommandBuffer;

	uint32_t imageIndex;
	VkResult result = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, frame.m_presentSemaphore, VK_NULL_HANDLE, &imageIndex);

	// 如果交换链失效（如窗口调整大小），后续需要处理，现在暂不实现重构逻辑
	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		recreateSwapchain();
		return;
	}

	vkResetFences(m_device, 1, &frame.m_renderFence);
	vkResetCommandPool(m_device, frame.m_commandPool, 0);

	m_editor->render_editor(this);
	float aspect = (float)m_swapchainExtent.width / (float)m_swapchainExtent.height;
	m_sceneData.view = m_camera.getViewMatrix();
	m_sceneData.proj = m_camera.getProjectionMatrix(aspect);

	// 2. 将数据写入本帧的 UBO 内存
	void* data;
	vmaMapMemory(m_allocator, frame.sceneBuffer.allocation, &data);
	memcpy(data, &m_sceneData, sizeof(GPUSceneData));
	vmaUnmapMemory(m_allocator, frame.sceneBuffer.allocation);

	// 录制简单的渲染指令
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	if (vkBeginCommandBuffer(frame.m_mainCommandBuffer, &beginInfo) != VK_SUCCESS) {
		throw std::runtime_error("failed to begin recording command buffer!");
	}

	std::array<VkClearValue, 2> clearValues{};
	clearValues[0].color = { {0.03f, 0.03f, 0.03f, 1.0f} }; // 颜色清除
	clearValues[1].depthStencil = { 1.0f, 0 };              // 深度清除


	transitionImageLayout(frame.m_mainCommandBuffer, m_viewportImage.image, m_swapchainImageFormat,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1);

	VkRenderPassBeginInfo sceneRpInfo{};
	sceneRpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	sceneRpInfo.renderPass = m_renderPass;
	sceneRpInfo.renderArea.offset = { 0, 0 };		//-------------
	sceneRpInfo.renderArea.extent = m_swapchainExtent;
	sceneRpInfo.framebuffer = m_viewportFramebuffer;
	sceneRpInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	sceneRpInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(frame.m_mainCommandBuffer, &sceneRpInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(frame.m_mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

		m_viewport.width = (float)m_swapchainExtent.width;
		m_viewport.height = (float)m_swapchainExtent.height;
		m_viewport.minDepth = 0.0f;
		m_viewport.maxDepth = 1.0f;
		vkCmdSetViewport(frame.m_mainCommandBuffer, 0, 1, &m_viewport);

		m_scissor.offset = { 0,0 };
		m_scissor.extent = m_swapchainExtent;
		vkCmdSetScissor(frame.m_mainCommandBuffer, 0, 1, &m_scissor);

		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &frame.sceneDescriptorSet, 0, nullptr);

		if (m_activeWorld) {
			drawWorld(frame.m_mainCommandBuffer, *m_activeWorld);
		}


	vkCmdEndRenderPass(frame.m_mainCommandBuffer);

	transitionImageLayout(frame.m_mainCommandBuffer, m_viewportImage.image, m_swapchainImageFormat,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1);

	std::array<VkClearValue, 2> uiClearValues{};
	uiClearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
	uiClearValues[1].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo uiRpInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	uiRpInfo.renderPass = m_uiRenderPass;
	uiRpInfo.renderArea.extent = m_swapchainExtent;
	uiRpInfo.framebuffer = m_frameBuffers[imageIndex]; // 渲染目标：真正的屏幕
	uiRpInfo.clearValueCount = 2;
	uiRpInfo.pClearValues = uiClearValues.data();

	vkCmdBeginRenderPass(frame.m_mainCommandBuffer, &uiRpInfo, VK_SUBPASS_CONTENTS_INLINE);

		// 录制编辑器的 UI 绘制指令
		m_editor->draw(frame.m_mainCommandBuffer);

	vkCmdEndRenderPass(frame.m_mainCommandBuffer);



	if (vkEndCommandBuffer(frame.m_mainCommandBuffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to record command buffer!");
	}

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &frame.m_presentSemaphore;	// 等待图像获取完毕
	submitInfo.pWaitDstStageMask = waitStages;

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &frame.m_mainCommandBuffer;

	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &frame.m_renderSemaphore;	// 发送渲染完毕信号

	if (vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, frame.m_renderFence) != VK_SUCCESS) {
		throw std::runtime_error("failed to submit draw command buffer!");
	}

	// 呈现图像
	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &frame.m_renderSemaphore; // 等待渲染完毕
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &m_swapchain;
	presentInfo.pImageIndices = &imageIndex;

	result = vkQueuePresentKHR(m_presentQueue, &presentInfo);
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_framebufferResized) {
		m_framebufferResized = false;
		recreateSwapchain();
	}

	ImGuiIO& io = ImGui::GetIO();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		vkQueueWaitIdle(m_graphicsQueue);
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}

	m_frameNumber++;
}

void ErisEngine::transitionImageLayout(VkCommandBuffer cmd, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout,uint32_t layerCount) {
	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = layerCount;

	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // 场景画完
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;        // UI 开始采样
	}
	else if ((oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL || oldLayout == VK_IMAGE_LAYOUT_UNDEFINED)
		&& newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) 
	{
		barrier.srcAccessMask = (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED) ? 0 : VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        sourceStage = (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED) ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	}
	else {
		std::cout << oldLayout << " " << newLayout << std::endl;
		throw std::invalid_argument("unsupported layout transition!");
	}

	vkCmdPipelineBarrier(cmd, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}


// ----------------------Handle External Events------------------------

void ErisEngine::handleInput(float deltaTime)
{
	// 1. 键盘移动逻辑 (WASD)
	glm::vec3 moveDir(0.0f);
	// 前后
	if (glfwGetKey(m_window, GLFW_KEY_W) == GLFW_PRESS) moveDir += m_camera.m_front;
	if (glfwGetKey(m_window, GLFW_KEY_S) == GLFW_PRESS) moveDir -= m_camera.m_front;
	// 左右 (通过叉乘计算右方向向量)
	glm::vec3 right = glm::normalize(glm::cross(m_camera.m_front, m_camera.m_up));
	if (glfwGetKey(m_window, GLFW_KEY_A) == GLFW_PRESS) moveDir -= right;
	if (glfwGetKey(m_window, GLFW_KEY_D) == GLFW_PRESS) moveDir += right;

	if (glm::length(moveDir) > 0.0f) {
		m_camera.processKeyboard(glm::normalize(moveDir), deltaTime);
	}

	// 2. 鼠标旋转逻辑 (模拟 UE5：按住右键时旋转)
	if (glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
		if (!m_isMousePressed) {
			// 初次按下时记录位置，防止视角跳变
			glfwGetCursorPos(m_window, &m_lastX, &m_lastY);
			m_isMousePressed = true;
			// 像编辑器一样隐藏鼠标，允许无限旋转
			glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		}

		double xpos, ypos;
		glfwGetCursorPos(m_window, &xpos, &ypos);

		float xoffset = static_cast<float>(xpos - m_lastX);
		float yoffset = static_cast<float>(ypos - m_lastY);

		m_lastX = xpos;
		m_lastY = ypos;

		m_camera.processMouse(xoffset, yoffset);
	}
	else {
		if (m_isMousePressed) {
			m_isMousePressed = false;
			// 释放右键后恢复鼠标显示
			glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}
	}
}

void ErisEngine::initViewportResources()
{
	// 1. 创建离屏图像 (m_viewportImage)
	VkFormat colorFormat= m_swapchainImageFormat;
	VkExtent3D extent = { m_swapchainExtent.width, m_swapchainExtent.height, 1 };

	VkImageCreateInfo imgInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	imgInfo.imageType = VK_IMAGE_TYPE_2D;
	imgInfo.format = colorFormat;
	imgInfo.extent = extent;
	imgInfo.mipLevels = 1;
	imgInfo.arrayLayers = 1;
	imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imgInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

	VmaAllocationCreateInfo allocationInfo{ VMA_MEMORY_USAGE_GPU_ONLY };
	if (vmaCreateImage(m_allocator, &imgInfo, &allocationInfo,
		&m_viewportImage.image, &m_viewportImage.allocation, nullptr) != VK_SUCCESS) {
		throw std::runtime_error("failed to create viewport image!");
	};

	m_viewportImage.imageView = createImageView(m_viewportImage.image, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);

	// 2. 创建视口 Framebuffer
	// 注意：这里必须包含 颜色附件(视口图) 和 深度附件(深度图)
	std::array<VkImageView, 2> attachments = { m_viewportImage.imageView, m_depthImage.imageView };

	VkFramebufferCreateInfo fbInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
	fbInfo.renderPass = m_renderPass;
	fbInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	fbInfo.pAttachments = attachments.data();
	fbInfo.width = m_swapchainExtent.width;
	fbInfo.height = m_swapchainExtent.height;
	fbInfo.layers = 1;

	// 关键点：确保赋值给类成员 m_viewportFramebuffer
	if (vkCreateFramebuffer(m_device, &fbInfo, nullptr, &m_viewportFramebuffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to create viewport framebuffer!");
	}

	immediateSubmit([&](VkCommandBuffer cmd) {
		transitionImageLayout(cmd, m_viewportImage.image, m_swapchainImageFormat,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,1);
		}); 

	if (m_viewportTextureSet != VK_NULL_HANDLE) {
		ImGui_ImplVulkan_RemoveTexture(m_viewportTextureSet);
	}

	// 3. 将离屏贴图交给 ImGui 注册
	// 注意：如果是重构交换链，ImGui 会处理旧的贴图 ID，或者你需要手动 RemoveTexture
	m_viewportTextureSet = ImGui_ImplVulkan_AddTexture(m_sampler, m_viewportImage.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void ErisEngine::cleanViewportResources()
{
	if (m_viewportFramebuffer != VK_NULL_HANDLE) {
		vkDestroyFramebuffer(m_device, m_viewportFramebuffer, nullptr);
		m_viewportFramebuffer = VK_NULL_HANDLE;
	}
	if (m_viewportImage.imageView != VK_NULL_HANDLE) {
		vkDestroyImageView(m_device, m_viewportImage.imageView, nullptr);
		m_viewportImage.imageView = VK_NULL_HANDLE;
	}
	if (m_viewportImage.image != VK_NULL_HANDLE) {
		vmaDestroyImage(m_allocator, m_viewportImage.image, m_viewportImage.allocation);
		m_viewportImage.image = VK_NULL_HANDLE;
	}
}


bool ErisEngine::loadMeshFromFile(const std::string& filename, Mesh& outMesh)
{
	Assimp::Importer importer;

	const aiScene* scene = importer.ReadFile(filename,
		aiProcess_Triangulate |
		aiProcess_FlipUVs |
		aiProcess_CalcTangentSpace |
		aiProcess_JoinIdenticalVertices
	);
	
	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
		std::cerr << "ASSIP ERROR: " << importer.GetErrorString() << std::endl;
		return false;
	}

	aiMesh* mesh = scene->mMeshes[0];

	outMesh.vertices.clear();
	outMesh.indices.clear();

	for (uint32_t i = 0; i < mesh->mNumVertices; i++) {
		Vertex vertex;
		vertex.pos = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
		
		if (mesh->HasNormals()) {
			vertex.normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };
		}
		if (mesh->mTextureCoords[0]) {
			vertex.uv = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
		}
		else {
			vertex.uv = { 0.0f, 0.0f };
		}

		// 颜色 (如果模型带顶点色，否则默认白色)
		if (mesh->HasVertexColors(0)) {
			vertex.color = { mesh->mColors[0][i].r, mesh->mColors[0][i].g, mesh->mColors[0][i].b };
		}
		else {
			vertex.color = { 1.0f, 1.0f, 1.0f };
		}

		outMesh.vertices.push_back(vertex);
	}

	for (uint32_t i = 0; i < mesh->mNumFaces; i++) {
		aiFace face = mesh->mFaces[i];
		for (uint32_t j = 0; j < face.mNumIndices; j++) {
			outMesh.indices.push_back(face.mIndices[j]);
		}
	}

	return true;
}



bool ErisEngine::loadModelFromFile(const std::string& filename, Model& outModel)
{
	Assimp::Importer importer;
	// 关键：PreTransformVertices 拍扁层级，FlipUVs 适配 Vulkan
	const aiScene* scene = importer.ReadFile(filename,
		aiProcess_Triangulate |
		aiProcess_FlipUVs |
		aiProcess_JoinIdenticalVertices |
		aiProcess_PreTransformVertices |
		aiProcess_GenSmoothNormals); // 确保如果没有法线则自动生成

	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
		std::cerr << "ASSIMP ERROR: " << importer.GetErrorString() << std::endl;
		return false;
	}

	// 获取文件夹路径用于加载贴图
	std::string directory = filename.substr(0, filename.find_last_of("\\/"));

	// std::cout << directory << std::endl;
	outModel.meshes.clear();
	outModel.materials.clear();

	// --- 第一步：加载所有材质 (MTL) ---
	// 预分配空间，防止 vector 扩容导致指针失效
	outModel.materials.reserve(scene->mNumMaterials);

	std::cout << scene->mNumMaterials << std::endl;

	for (uint32_t i = 0; i < scene->mNumMaterials; i++) {
		aiMaterial* aiMat = scene->mMaterials[i];
		Material engineMat;
		//engineMat.textureSet = m_defaultTextureSet;

		// 1. 获取 Diffuse 颜色 (Kd)
		//aiColor3D diffuseColor(1.0f, 1.0f, 1.0f);
		//aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor);
		// 我们暂时将颜色存入每一个顶点（见下文网格加载环节）

		// 2. 加载贴图 (map_Kd)
		aiString path;
		if (aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &path) == AI_SUCCESS) {
			std::string dir = filename.substr(0, filename.find_last_of("\\/"));
			std::string fullTexPath = dir + "/" + path.C_Str();

			std::cout << fullTexPath << std::endl;

			engineMat.diffuseTexture = loadImageFromFile(fullTexPath.c_str());
			engineMat.textureSet = createDescriptorSet(engineMat.diffuseTexture);
		}
		

		outModel.materials.push_back(engineMat);
	}

	outModel.minBound = glm::vec3(FLT_MAX);
	outModel.maxBound = glm::vec3(-FLT_MAX);
	// --- 第二步：加载所有网格 (Mesh) ---
	for (unsigned int m = 0; m < scene->mNumMeshes; m++) {
		aiMesh* aiMeshPtr = scene->mMeshes[m];
		Mesh localMesh;

		// 获取对应的材质颜色作为顶点底色
		aiMaterial* aiMat = scene->mMaterials[aiMeshPtr->mMaterialIndex];
		aiColor3D kd(1.0f, 1.0f, 1.0f);
		aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, kd);


		// 填充顶点
		for (uint32_t i = 0; i < aiMeshPtr->mNumVertices; i++) {
			Vertex v;
			v.pos = { aiMeshPtr->mVertices[i].x, aiMeshPtr->mVertices[i].y, aiMeshPtr->mVertices[i].z };
			localMesh.minBound = glm::min(localMesh.minBound, v.pos);
			localMesh.maxBound = glm::max(localMesh.maxBound, v.pos);
			if (aiMeshPtr->HasNormals()) {
				v.normal = { aiMeshPtr->mNormals[i].x, aiMeshPtr->mNormals[i].y, aiMeshPtr->mNormals[i].z };
			}

			if (aiMeshPtr->mTextureCoords[0]) {
				v.uv = { aiMeshPtr->mTextureCoords[0][i].x, aiMeshPtr->mTextureCoords[0][i].y };
			}
			else {
				v.uv = { 0.0f, 0.0f };
			}

			// 颜色：使用 MTL 的 Kd 颜色
			v.color = { kd.r, kd.g, kd.b };

			localMesh.vertices.push_back(v);
		}

		// 填充索引
		for (uint32_t i = 0; i < aiMeshPtr->mNumFaces; i++) {
			aiFace face = aiMeshPtr->mFaces[i];
			for (uint32_t j = 0; j < face.mNumIndices; j++) {
				localMesh.indices.push_back(face.mIndices[j]);
			}
		}

		// 关联材质指针
		if (aiMeshPtr->mMaterialIndex < outModel.materials.size()) {
			localMesh.material = &outModel.materials[aiMeshPtr->mMaterialIndex];
		}

		outModel.minBound = glm::min(outModel.minBound, localMesh.minBound);
		outModel.maxBound = glm::max(outModel.maxBound, localMesh.maxBound);
		outModel.meshes.push_back(localMesh);
	}


	// --- 第三步：上传 GPU ---
	for (auto& m : outModel.meshes) {
		m.uploadMesh(m_allocator, this);
	}

	std::cout << "[Engine] Successfully loaded " << filename << " with " << outModel.meshes.size() << " parts." << std::endl;
	return true;
}

AllocatedImage ErisEngine::loadImageFromFile(const char* file)
{
	int texWidth, texHeight, texChannels;

	// 1. 使用 stb_image 加载像素数据（强制为 4 通道 RGBA）
	stbi_uc* pixels = stbi_load(file, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	if (!pixels) {
		std::cerr << "Failed to load texture file: " << file << std::endl;
		// 返回一个空的句柄，防止崩溃
		return AllocatedImage{ VK_NULL_HANDLE, VK_NULL_HANDLE, nullptr };
	}

	VkDeviceSize imageSize = texWidth * texHeight * 4;
	VkFormat imageFormat = VK_FORMAT_R8G8B8A8_SRGB; // 颜色贴图建议使用 SRGB

	// 2. 创建暂存缓冲 (Staging Buffer)
	VkBufferCreateInfo stagingInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	stagingInfo.size = imageSize;
	stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	VmaAllocationCreateInfo stagingAllocInfo{};
	stagingAllocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

	AllocatedBuffer stagingBuffer;
	vmaCreateBuffer(m_allocator, &stagingInfo, &stagingAllocInfo, &stagingBuffer.buffer, &stagingBuffer.allocation, nullptr);

	// 拷贝像素数据到暂存缓冲
	void* data;
	vmaMapMemory(m_allocator, stagingBuffer.allocation, &data);
	memcpy(data, pixels, static_cast<size_t>(imageSize));
	vmaUnmapMemory(m_allocator, stagingBuffer.allocation);
	stbi_image_free(pixels); // 释放内存中的像素

	// 3. 创建 GPU 图像 (VkImage)
	VkExtent3D imageExtent;
	imageExtent.width = static_cast<uint32_t>(texWidth);
	imageExtent.height = static_cast<uint32_t>(texHeight);
	imageExtent.depth = 1;

	VkImageCreateInfo dimg_info{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	dimg_info.imageType = VK_IMAGE_TYPE_2D;
	dimg_info.format = imageFormat;
	dimg_info.extent = imageExtent;
	dimg_info.mipLevels = 1;
	dimg_info.arrayLayers = 1;
	dimg_info.samples = VK_SAMPLE_COUNT_1_BIT;
	dimg_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	dimg_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT; // 既能接收拷贝，又能被 Shader 采样

	VmaAllocationCreateInfo dimg_allocinfo{};
	dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	AllocatedImage newImage;
	vmaCreateImage(m_allocator, &dimg_info, &dimg_allocinfo, &newImage.image, &newImage.allocation, nullptr);

	// 4. 将数据从 Buffer 拷贝到 Image (利用 immediateSubmit)
	immediateSubmit([&](VkCommandBuffer cmd) {
		// A. 转换布局：Undefined -> Transfer Destination (准备接收数据)
		transitionImageLayout(cmd, newImage.image, imageFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1);

		// B. 执行拷贝
		VkBufferImageCopy copyRegion{};
		copyRegion.bufferOffset = 0;
		copyRegion.bufferRowLength = 0;
		copyRegion.bufferImageHeight = 0;
		copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.imageSubresource.mipLevel = 0;
		copyRegion.imageSubresource.baseArrayLayer = 0;
		copyRegion.imageSubresource.layerCount = 1;
		copyRegion.imageExtent = imageExtent;

		vkCmdCopyBufferToImage(cmd, stagingBuffer.buffer, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

		// C. 转换布局：Transfer Destination -> Shader Read Only (准备给 Shader 用)
		transitionImageLayout(cmd, newImage.image, imageFormat, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,1);
		});

	// 5. 创建 Image View
	newImage.imageView = createImageView(newImage.image, imageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);

	// 6. 清理暂存缓冲
	vmaDestroyBuffer(m_allocator, stagingBuffer.buffer, stagingBuffer.allocation);

	return newImage;
}

void ErisEngine::initDefaultResources() {
	// 1. 定义一个纯白色像素 (RGBA)
	uint32_t whitePixel = 0xFFFFFFFF;

	// 2. 创建暂存缓冲 (Staging Buffer) - 必须确保 CPU 可见
	VkBufferCreateInfo stagingInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	stagingInfo.size = 4;
	stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	VmaAllocationCreateInfo stagingAllocInfo{};
	// 修正点：对于这种极小的暂存缓冲，使用 CPU_ONLY 是最稳健的，保证 HOST_VISIBLE 和 HOST_COHERENT
	stagingAllocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

	AllocatedBuffer stagingBuffer;
	// 这里不需要传 VmaAllocationInfo 了，因为我们后面手动 Map
	if (vmaCreateBuffer(m_allocator, &stagingInfo, &stagingAllocInfo,
		&stagingBuffer.buffer, &stagingBuffer.allocation, nullptr) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create staging buffer for default texture");
	}

	// 拷贝数据
	void* data;
	vmaMapMemory(m_allocator, stagingBuffer.allocation, &data);
	memcpy(data, &whitePixel, 4);
	vmaUnmapMemory(m_allocator, stagingBuffer.allocation);

	// 3. 创建 1x1 GPU 图像
	VkExtent3D extent = { 1, 1, 1 };
	VkImageCreateInfo imgInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	imgInfo.imageType = VK_IMAGE_TYPE_2D;
	imgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	imgInfo.extent = extent;
	imgInfo.mipLevels = 1;
	imgInfo.arrayLayers = 1;
	imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

	VmaAllocationCreateInfo gpuImageAllocInfo{};
	gpuImageAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY; // 图像数据仅 GPU 访问

	if (vmaCreateImage(m_allocator, &imgInfo, &gpuImageAllocInfo,
		&m_defaultTexture.image, &m_defaultTexture.allocation, nullptr) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create 1x1 default image");
	}

	// 4. 利用之前写好的 transitionImageLayout 进行布局转换和拷贝
	immediateSubmit([&](VkCommandBuffer cmd) {
		// A. 转换布局为接收端
		transitionImageLayout(cmd, m_defaultTexture.image, VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1);

		// B. 拷贝缓冲区到图像
		VkBufferImageCopy copyRegion{};
		copyRegion.bufferOffset = 0;
		copyRegion.bufferRowLength = 0;
		copyRegion.bufferImageHeight = 0;
		copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.imageSubresource.mipLevel = 0;
		copyRegion.imageSubresource.baseArrayLayer = 0;
		copyRegion.imageSubresource.layerCount = 1;
		copyRegion.imageExtent = extent;

		vkCmdCopyBufferToImage(cmd, stagingBuffer.buffer, m_defaultTexture.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

		// C. 转换为 Shader 可读布局
		transitionImageLayout(cmd, m_defaultTexture.image, VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1);
		});

	// 5. 创建视图
	m_defaultTexture.imageView = createImageView(m_defaultTexture.image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1);

	// 6. 创建默认描述符集 (这个函数你应该已经实现好了)
	m_defaultTextureSet = createDescriptorSet(m_defaultTexture);

	// 7. 清理暂存缓冲 (临时工，立即销毁)
	vmaDestroyBuffer(m_allocator, stagingBuffer.buffer, stagingBuffer.allocation);

	// 8. 注册保底贴图的销毁逻辑到 DeletionQueue
	m_mainDeletionQueue.push_function([=]() {
		vkDestroyImageView(m_device, m_defaultTexture.imageView, nullptr);
		vmaDestroyImage(m_allocator, m_defaultTexture.image, m_defaultTexture.allocation);
		});
}

void ErisEngine::initSceneData()
{
	memset(&m_sceneData, 0, sizeof(GPUSceneData));
	m_sceneData.fogColor = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
	m_sceneData.ambientColor = glm::vec4(0.2f, 0.2f, 0.2f, 1.0f);
	m_sceneData.sunlightDir = glm::vec4(0.5f, 1.0f, 0.3f, 1.0f);
	m_sceneData.sunlightColor = glm::vec4(1.0f, 0.9f, 0.8f, 1.0f);
	m_sceneData.lightCount = 0; // 初始没有点光源
}



// ---------------------------World Functions---------------------------------


Model* ErisEngine::getOrLoadModel(const std::string& path) {
	// 1. 如果模型已经在池子里了，直接返回指针
	if (m_assetLibrary.find(path) != m_assetLibrary.end()) {
		return &m_assetLibrary[path];
	}

	// 2. 创建新 Model 容器
	Model& newModel = m_assetLibrary[path];

	// 3. 执行真正的磁盘加载
	if (loadModelFromFile(path, newModel)) {
		// 4. 上传到 GPU（调用你之前的 uploadModel）
		newModel.uploadModel(m_allocator, this);
		return &newModel;
	}

	// 5. 加载失败则清理 map
	m_assetLibrary.erase(path);
	std::cerr << "Failed to import model: " << path << std::endl;
	return nullptr;
}

void ErisEngine::drawWorld(VkCommandBuffer cmd, ErisWorld& world) {

	FrameData& frame = getCurrentFrame();
	
	// 1. 获取通用的 View 和 Projection（一帧只需计算一次）
	float aspect = (float)m_swapchainExtent.width / (float)m_swapchainExtent.height;
	glm::mat4 projection = m_camera.getProjectionMatrix(aspect);
	glm::mat4 view = m_camera.getViewMatrix();
	

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
		m_pipelineLayout, 0, 1, &frame.sceneDescriptorSet, 0, nullptr);

	// 2. 遍历世界里所有的物体
	for (auto& objPtr : world.getObjects()) {
		RenderObject& obj = *objPtr;
		if (!obj.m_pModel) continue;

		glm::mat4 modelMatrix = obj.getTransformMatrix();
		MeshPushConstants constants;
		constants.render_matrix = projection * view * modelMatrix;
		constants.model_matrix = modelMatrix;

		// 推送当前物体的矩阵
		vkCmdPushConstants(cmd, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants), &constants);


		// --- 这里是原本遍历 mesh 的循环 ---
		for (auto& mesh : obj.m_pModel->meshes) {
			if (mesh.vertexBuffer.buffer == VK_NULL_HANDLE) continue;

			// 绑定描述符集 (Set 0)
			VkDescriptorSet set_to_bind = (mesh.material && mesh.material->textureSet != VK_NULL_HANDLE)
				? mesh.material->textureSet : m_defaultTextureSet;


			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 
				m_pipelineLayout, 1, 1, &set_to_bind, 0, nullptr);

			// 绑定顶点和索引
			VkBuffer vBuffers[] = { mesh.vertexBuffer.buffer };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(cmd, 0, 1, vBuffers, offsets);
			vkCmdBindIndexBuffer(cmd, mesh.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
			// 绘制
			vkCmdDrawIndexed(cmd, static_cast<uint32_t>(mesh.indices.size()), 1, 0, 0, 0);

		}
	}

	// ---------------------------------------------------------
	// --- 3. 绘制天空盒 (放在物体之后，利用深度测试) ---
	// ---------------------------------------------------------
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_skyboxPipeline);

	// 绑定天空盒贴图 (Set 1)
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_skyboxPipelineLayout, 
		0, 1, &frame.sceneDescriptorSet, 0, nullptr);

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_skyboxPipelineLayout,
		1, 1, &m_skyboxDescriptorSet, 0, nullptr);

	// 绑定天空盒的顶点数据 (m_skyboxMesh 应该是你之前生成的立方体)
	VkBuffer skyBuffers[] = { m_skyboxMesh.vertexBuffer.buffer };
	VkDeviceSize skyOffsets[] = { 0 };
	vkCmdBindVertexBuffers(cmd, 0, 1, skyBuffers, skyOffsets);
	vkCmdBindIndexBuffer(cmd, m_skyboxMesh.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

	// 绘制 36 个索引（立方体）
	vkCmdDrawIndexed(cmd, 36, 1, 0, 0, 0);

	// ---------------------------------------------------------
   // 2. 【新增】：渲染无限网格
   // ---------------------------------------------------------
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gridPipeline);

	// 只绑定 Set 0 (全局 UBO)
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gridPipelineLayout, 0, 1, &frame.sceneDescriptorSet, 0, nullptr);

	// 因为顶点在 Shader 里，所以直接画 6 个点即可
	vkCmdDraw(cmd, 6, 1, 0, 0);
}

RenderObject* ErisEngine::pickObject(float mouseX, float mouseY)
{
	// mouseX, mouseY 是你从 ImGui 传进来的 [0.0, 1.0] 的相对比例坐标

	// 1. 转换为 NDC (Normalized Device Coordinates)
	// Vulkan 的 NDC: x 在 [-1, 1], y 在 [-1, 1]
	float x_ndc = (mouseX * 2.0f) - 1.0f;
	float y_ndc = (mouseY * 2.0f) - 1.0f;

	// 2. 获取相机矩阵
	float aspect = (float)m_swapchainExtent.width / (float)m_swapchainExtent.height;
	glm::mat4 projection = m_camera.getProjectionMatrix(aspect);
	glm::mat4 view = m_camera.getViewMatrix();

	// 【重要：Vulkan 特有的逆矩阵处理】
	// 渲染时我们为了适配屏幕做了 projection[1][1] *= -1;
	// 在计算射线时，这个翻转必须保留，因为我们要和渲染出的画面完全对齐！
	glm::mat4 invVP = glm::inverse(projection * view);

	// 3. 生成世界空间中的射线
	// Vulkan 近平面 z = 0.0, 远平面 z = 1.0 (这是 Vulkan 和 OpenGL 最大的区别！)
	glm::vec4 screenPosNear(x_ndc, y_ndc, 0.0f, 1.0f);
	glm::vec4 screenPosFar(x_ndc, y_ndc, 1.0f, 1.0f);

	// 通过逆矩阵转回世界空间
	glm::vec4 worldPosNear = invVP * screenPosNear;
	glm::vec4 worldPosFar = invVP * screenPosFar;

	// 透视除法
	worldPosNear /= worldPosNear.w;
	worldPosFar /= worldPosFar.w;

	// 得到世界空间射线
	glm::vec3 rayOrigin = glm::vec3(worldPosNear);
	glm::vec3 rayDir = glm::normalize(glm::vec3(worldPosFar) - rayOrigin);

	// 4. 遍历检测
	RenderObject* closestObj = nullptr;
	float minDistance = FLT_MAX;

	for (auto& objPtr : m_activeWorld->getObjects()) {
		glm::vec3 bMin, bMax;
		// 获取物体在世界空间（已经乘过 Model 矩阵）的包围盒
		objPtr->getWorldBounds(bMin, bMax);

		float t;
		// 在【世界空间】执行射线与 AABB 的求交
		if (m_activeWorld->rayIntersectAABB(rayOrigin, rayDir, bMin, bMax, t)) {
			if (t < minDistance) {
				minDistance = t;
				closestObj = objPtr.get();
			}
		}
	}

	return closestObj;
}
