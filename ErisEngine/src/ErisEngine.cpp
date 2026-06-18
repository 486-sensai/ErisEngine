#include "ErisEngine.h"
#include "LayoutTransition.h"

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
	glfwSetWindowUserPointer(m_window, this);				// �󶨵�ǰ����ָ��
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
	initViewportPass();
	initUIRenderPass();
	createFramebuffers();

	initGbuffer();
	initSyncStructures();
	initCommands();

	initDescriptors();
	initDescriptorPool();

	initForwardPipeline();
	initLumenGbufferPipeline();
	initLumenLightingPipeline();

	initSkyboxPipeline();
	initGridPipeline();
	initShadowResources();
	initShadowPipeline();
	createSceneBuffers();
	initSkyboxMesh();

	m_activeWorld = new ErisWorld();
	m_activeWorld->getPhysics().createInfinitePlane();

	m_activeWorld->createDefaultSkybox();
	m_skyboxImage = loadCubemap(m_activeWorld->skyboxFaces);
	updateSkyboxDescriptor();
	updateLumenDescriptorSet();

	m_editor = new ErisEditor();
	m_editor->init(this);
	initViewportResources();
	// ���û�м��س���������1x1��ɫ����
	initDefaultResources();
	// ��ʼ�������
	initSceneData();


	Model* pSportCar = getOrLoadModel("assets/sportsCar/sportsCar.obj");
	Model* pGroundAsset = getOrLoadModel("assets/ground/churchfloor.obj");
	// set false
	if (!pGroundAsset) {
		RenderObject* ground = m_activeWorld->spawnObject(pGroundAsset, "Main_Ground");

		ground->m_location = glm::vec3(0.0f, -0.01f, 0.0f);

		ground->m_scale = glm::vec3(1.0f, 1.0f, 1.0f);

		ground->m_initialLocation = ground->m_location;
	}

	// debug�� ���Զ���
	{
		// ���������������ڲ�ͬλ��
		RenderObject* car1 = m_activeWorld->spawnObject(pSportCar, "Main_Car");
		car1->m_location = glm::vec3(0.0f, 0.0f, 0.0f);

		RenderObject* car2 = m_activeWorld->spawnObject(pSportCar, "Backup_Car");
		car2->m_initialLocation = car2->m_location = glm::vec3(5.0f, 0.0f, -2.0f);
		car2->m_scale = glm::vec3(0.5f);

	}
	m_camera.m_position = glm::vec3(0.0f, 1.0f, 3.0f);



	m_isInitialized = true;
	return 1;

}

// �����չ�Ƿ�֧�ֽ�����
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
	// �ж���ѡ����Ƿ����
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
	
	// 1. ���� 6 ��ͼ
	for (int i = 0; i < 6; i++) {
		pixels[i] = stbi_load(faces[i].c_str(), &width, &height, &channels, STBI_rgb_alpha);
		if (!pixels[i])throw std::runtime_error("failed to load cubemap face!");
	}

	VkDeviceSize layerSize = width * height * 4;
	VkDeviceSize totalSize = layerSize * 6;

	uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;

	VkFormat imageFormat = VK_FORMAT_R8G8B8A8_SRGB; 
	VkExtent3D imageExtent;
	imageExtent.width = static_cast<uint32_t>(width);
	imageExtent.height = static_cast<uint32_t>(height);
	imageExtent.depth = 1;

	// 2. ���� Staging Buffer
	AllocatedBuffer stagingbuffer = createBuffer(totalSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	void* data;
	vmaMapMemory(m_allocator, stagingbuffer.allocation, &data);
		for (int i = 0; i < 6; i++) {
			memcpy(static_cast<char*>(data) + (layerSize * i), pixels[i], layerSize);
			stbi_image_free(pixels[i]);
		}
	vmaUnmapMemory(m_allocator, stagingbuffer.allocation);

	// 3. ���� GPU Image (�ؼ���arrayLayers = 6, flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT)
	AllocatedImage newImage;
	VkImageCreateInfo imgInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	imgInfo.imageType = VK_IMAGE_TYPE_2D;
	imgInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
	imgInfo.extent = imageExtent;
	imgInfo.mipLevels = mipLevels;
	imgInfo.arrayLayers = 6;
	imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imgInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;


	VmaAllocationCreateInfo vmaAllocInfo{ VMA_MEMORY_USAGE_GPU_ONLY };
	vmaCreateImage(m_allocator, &imgInfo, &vmaAllocInfo, &newImage.image, &newImage.allocation, nullptr);

	
	immediateSubmit([&](VkCommandBuffer cmd) {
		transitionImageLayout(cmd, newImage.image, imageFormat,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			6, mipLevels, 0);

		std::vector<VkBufferImageCopy> regions(6);
		for (uint32_t i = 0; i < 6; i++) {
			regions[i].bufferOffset = layerSize * i;
			regions[i].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			regions[i].imageSubresource.mipLevel = 0;
			regions[i].imageSubresource.baseArrayLayer = i;
			regions[i].imageSubresource.layerCount = 1;
			regions[i].imageExtent = imageExtent;
		}

		vkCmdCopyBufferToImage(cmd, stagingbuffer.buffer, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 6, regions.data());

		// C. ѭ������ Mipmaps
		int32_t mipWidth = width;
		int32_t mipHeight = height;

		for (uint32_t i = 1; i < mipLevels; i++) {
			// 1. ����һ�� (i-1) �� DST תΪ SRC ��������ȡ
			// layerCount=6, mipLevels=1, baseMipLevel=i-1
			transitionImageLayout(cmd, newImage.image, imageFormat,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				6, 1, i - 1);

			// 2. ִ�� Blit (6 ����һ��ѹ��)
			VkImageBlit blit{};
			blit.srcOffsets[0] = { 0, 0, 0 };
			blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
			blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.srcSubresource.mipLevel = i - 1;
			blit.srcSubresource.baseArrayLayer = 0;
			blit.srcSubresource.layerCount = 6;

			blit.dstOffsets[0] = { 0, 0, 0 };
			blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
			blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.dstSubresource.mipLevel = i;
			blit.dstSubresource.baseArrayLayer = 0;
			blit.dstSubresource.layerCount = 6;

			vkCmdBlitImage(cmd, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

			// 3. ��һ�� (i-1) �Ѿ����꣬��ʽ����תΪ���յ� Shader �ɶ�״̬
			transitionImageLayout(cmd, newImage.image, imageFormat,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				6, 1, i - 1);

			if (mipWidth > 1) mipWidth /= 2;
			if (mipHeight > 1) mipHeight /= 2;
		}

		// D. �����һ�����ɵ���С Mip ��תΪ Shader �ɶ�
		transitionImageLayout(cmd, newImage.image, imageFormat,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			6, 1, mipLevels - 1);
		});


	// 5. ��������� ImageView (viewType = VK_IMAGE_VIEW_TYPE_CUBE)
	VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	viewInfo.image = newImage.image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
	viewInfo.format = imageFormat;
	viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, 6 };
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
	createInfo.minImageCount = imageCount;	// ���ػ���
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	// imageArrayLayers ָ��ÿ��ͼ������Ĳ����� ���������ڿ������� 3D Ӧ�ó��򣬷����ֵʼ��Ϊ 1

	createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;		// ��ֱͬ��
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
		createInfo.pQueueFamilyIndices = nullptr;   //optional(���Բ���д������)
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

	while (width == 0 || height == 0) { // ������С��
		glfwGetFramebufferSize(m_window, &width, &height);
		glfwWaitEvents();
	}

	vkDeviceWaitIdle(m_device);

	cleanSwapchain();


	// 2. ���´���
	createSwapchain();
	createImageViews();
	createDepthBuffer();    // �������´�����Ȼ��壬��Ϊ�����ֱ������
	createFramebuffers();   // ���°�
	initShadowResources();
	createSceneBuffers();
	initViewportResources();
	initGbuffer();
	updateLumenDescriptorSet();
	ImGui_ImplVulkan_SetMinImageCount(static_cast<uint32_t>(m_swapchainImages.size()));
}

// ����ͼ����ͼ
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

		framebufferInfo.renderPass = m_uiRenderPass;
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
		// 1. �����Դ��еĻ����� (CPU_TO_GPU ģʽ����ÿ֡����)
		m_frames[i].sceneBuffer = createBuffer(sizeof(GPUSceneData),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

		// 2. ���� Set 0 (ȫ��������)
		VkDescriptorSetAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		allocInfo.descriptorPool = m_descriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &m_globalSetLayout;
		vkAllocateDescriptorSets(m_device, &allocInfo, &m_frames[i].sceneDescriptorSet);

		// 3. �� Buffer д�루�����������
		VkDescriptorBufferInfo binfo = {};
		binfo.buffer = m_frames[i].sceneBuffer.buffer;
		binfo.offset = 0;
		binfo.range = sizeof(GPUSceneData);

		VkWriteDescriptorSet uboWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		uboWrite.dstSet = m_frames[i].sceneDescriptorSet;
		uboWrite.dstBinding = 0;
		uboWrite.descriptorCount = 1;
		uboWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboWrite.pBufferInfo = &binfo;

		VkDescriptorImageInfo shadowImageInfo{};
		shadowImageInfo.sampler = m_skyboxSampler;
		shadowImageInfo.imageView = m_shadowImage.imageView;
		shadowImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkWriteDescriptorSet shadowWrite{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		shadowWrite.dstSet = m_frames[i].sceneDescriptorSet;
		shadowWrite.dstBinding = 1;											// ��Ӧ Shader ��� layout(set=0, binding=1)
		shadowWrite.descriptorCount = 1;
		shadowWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		shadowWrite.pImageInfo = &shadowImageInfo;

		std::array<VkWriteDescriptorSet, 2> writes = { uboWrite,shadowWrite };
		vkUpdateDescriptorSets(m_device, 2, writes.data(), 0, nullptr);
	}



}

void ErisEngine::updateSkyboxDescriptor()
{
	VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	allocInfo.descriptorPool = m_descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &m_skyboxDescriptorSetLayout; // �ؼ���ʹ����պв���

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

// �õ����Ķ��ز���
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

void ErisEngine::initForwardPipeline()
{
	VkShaderModule vertModule, fragModule;
	if (!loadShaderModule("shaders/vert.spv", &vertModule) || !loadShaderModule("shaders/frag.spv", &fragModule)) {
		throw std::runtime_error("failed to load shaders!");
	}

	std::array<VkDescriptorSetLayout, 2> layouts = { m_globalSetLayout,m_singleTextureLayout };

	// ���߲���
	VkPushConstantRange pushConstantRange;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(MeshPushConstants);
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

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
	// 1. ������ɫ���׶�
	builder.m_shaderStages.push_back({ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertModule, "main" });
	builder.m_shaderStages.push_back({ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragModule, "main" });


	// 2. ���ö������루�����ǿյģ�
	builder.m_vertexBindings.push_back(Vertex::getBindingDescription());
	builder.m_vertexAttributes = Vertex::getAttributeDescriptions();

	builder.m_vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	builder.m_vertexInputInfo.pNext = nullptr;

	// 3. �������ˣ������Σ�
	builder.m_inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	builder.m_inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	builder.m_inputAssembly.primitiveRestartEnable = VK_FALSE;

	// 4. �����ӿ�
	builder.m_dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	builder.m_viewport = { 0.0f,0.0f,(float)m_swapchainExtent.width,(float)m_swapchainExtent.height,0.0f,1.0f };
	builder.m_scissor = { {0,0},m_swapchainExtent };

	// 5. ��դ��
	builder.m_rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	builder.m_rasterizer.pNext = nullptr;
	builder.m_rasterizer.flags = 0;
	builder.m_rasterizer.depthClampEnable = VK_FALSE;
	builder.m_rasterizer.rasterizerDiscardEnable = VK_FALSE;
	builder.m_rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	builder.m_rasterizer.cullMode = VK_CULL_MODE_NONE;
	builder.m_rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;	// vulkan��תY����
	builder.m_rasterizer.depthBiasEnable = VK_FALSE;
	builder.m_rasterizer.depthBiasConstantFactor = 0.0f;
	builder.m_rasterizer.depthBiasClamp = 0.0f;
	builder.m_rasterizer.depthBiasSlopeFactor = 0.0f;
	builder.m_rasterizer.lineWidth = 1.0f;

	// 6. ���ģʽ����͸����
	VkPipelineColorBlendAttachmentState blend{};
	blend.blendEnable = VK_FALSE;
	blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	builder.m_colorBlendAttachments.clear();
	builder.m_colorBlendAttachments.push_back(blend);

	// --- �������ӣ���Ȳ������� ---
	builder.m_depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	builder.m_depthStencil.pNext = nullptr;
	builder.m_depthStencil.depthTestEnable = VK_TRUE;           // ������Ȳ���
	builder.m_depthStencil.depthWriteEnable = VK_TRUE;          // �������д��
	builder.m_depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL; // Խ��������Խ�Ȼ�
	builder.m_depthStencil.depthBoundsTestEnable = VK_FALSE;
	builder.m_depthStencil.minDepthBounds = 0.0f;
	builder.m_depthStencil.maxDepthBounds = 1.0f;
	builder.m_depthStencil.stencilTestEnable = VK_FALSE;        // ��ʱ������ģ�����

	// 7. ���ز���
	builder.m_multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	builder.m_multisampling.sampleShadingEnable = VK_FALSE;
	builder.m_multisampling.rasterizationSamples = msaaSamples;

	builder.m_pipelineLayout = m_pipelineLayout;
	m_pipeline = builder.buildPipeline(m_device, m_viewportPass);

	
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
	// ��պ�ͨ������Ҫ PushConstants������ֻ��Ҫһ������ϵ��
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
	skyBuilder.m_rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	skyBuilder.m_rasterizer.lineWidth = 1.0f;

	skyBuilder.m_multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	skyBuilder.m_multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState skyBlend{};
	skyBlend.blendEnable = VK_FALSE;
	skyBlend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	skyBuilder.m_colorBlendAttachments.clear();
	skyBuilder.m_colorBlendAttachments.push_back(skyBlend);

	skyBuilder.m_depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	skyBuilder.m_depthStencil.depthTestEnable = VK_TRUE;
	skyBuilder.m_depthStencil.depthWriteEnable = VK_FALSE; // �ؼ�����պв�д����ȣ����ڵ�����UI
	skyBuilder.m_depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

	skyBuilder.m_pipelineLayout = m_skyboxPipelineLayout;

	m_skyboxPipeline = skyBuilder.buildPipeline(m_device, m_viewportPass);
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
	if (!loadShaderModule("shaders/grid_vert.spv", &vertMod) || !loadShaderModule("shaders/grid_frag.spv", &fragMod)) {
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

	VkPipelineColorBlendAttachmentState blendAttachment{};
	blendAttachment.blendEnable = VK_TRUE;
	blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendAttachmentState gridBlend{}; 
	gridBlend.blendEnable = VK_TRUE;
	gridBlend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	gridBlend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	gridBlend.colorBlendOp = VK_BLEND_OP_ADD;
	gridBlend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	gridBlend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	gridBlend.alphaBlendOp = VK_BLEND_OP_ADD;
	gridBlend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	builder.m_colorBlendAttachments.clear();
	builder.m_colorBlendAttachments.push_back(gridBlend);

	builder.m_depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	builder.m_depthStencil.depthTestEnable = VK_TRUE;  // ���뿪�������ڼ���Ƿ񱻳��ڵ�
	builder.m_depthStencil.depthWriteEnable = VK_FALSE; // ����رգ���Ϊ������͸����
	builder.m_depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

	builder.m_pipelineLayout = m_gridPipelineLayout;
	m_gridPipeline = builder.buildPipeline(m_device, m_viewportPass);

	vkDestroyShaderModule(m_device, vertMod, nullptr);
	vkDestroyShaderModule(m_device, fragMod, nullptr);

	m_mainDeletionQueue.push_function([=]() {
		vkDestroyPipelineLayout(m_device, m_gridPipelineLayout, nullptr);
		vkDestroyPipeline(m_device, m_gridPipeline, nullptr);
		});

}

void ErisEngine::initShadowResources()
{
	m_shadowExtent = { 2048, 2048 }; // ��Ӱͼ�ߴ�

	// 1. ������Ӱ���ͼ�� (ʹ�� VMA)
	VkImageCreateInfo imgInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	imgInfo.imageType = VK_IMAGE_TYPE_2D;
	imgInfo.format = m_depthFormat; // ���� D32_SFLOAT
	imgInfo.extent = { m_shadowExtent.width, m_shadowExtent.height, 1 };
	imgInfo.mipLevels = 1;
	imgInfo.arrayLayers = 1;
	imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	// �ؼ���������ȸ�������Ҫ��Ϊ��ͼ������
	imgInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;


	VmaAllocationCreateInfo vmaAllocInfo{ VMA_MEMORY_USAGE_GPU_ONLY };
	vmaCreateImage(m_allocator, &imgInfo, &vmaAllocInfo,
		&m_shadowImage.image, &m_shadowImage.allocation, nullptr);

	// 2. ����ͼ����ͼ
	m_shadowImage.imageView = createImageView(m_shadowImage.image, m_depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);

	// 3. ������Ӱ RenderPass (���������)
	VkAttachmentDescription depthAttachment{};
	depthAttachment.format = m_depthFormat;
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;    // ÿһ֡��ʼ���
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // ���������� Pass ��ȡ
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // ����ֱ�ӽ����ȡ״̬

	VkAttachmentReference depthRef = { 0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.pDepthStencilAttachment = &depthRef;

	VkRenderPassCreateInfo rpInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	rpInfo.attachmentCount = 1;
	rpInfo.pAttachments = &depthAttachment;
	rpInfo.subpassCount = 1;
	rpInfo.pSubpasses = &subpass;

	if (vkCreateRenderPass(m_device, &rpInfo, nullptr, &m_shadowRenderPass) != VK_SUCCESS) {
		throw std::runtime_error("failed to create shadow render pass!");
	}

	// 4. ������Ӱ Framebuffer
	VkFramebufferCreateInfo fbInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
	fbInfo.renderPass = m_shadowRenderPass;
	fbInfo.attachmentCount = 1;
	fbInfo.pAttachments = &m_shadowImage.imageView;
	fbInfo.width = m_shadowExtent.width;
	fbInfo.height = m_shadowExtent.height;
	fbInfo.layers = 1;

	vkCreateFramebuffer(m_device, &fbInfo, nullptr, &m_shadowFrameBuffer);

	// 5. ע������
	// cleanup moved to cleanSwapchain()
}

void ErisEngine::initShadowPipeline()
{
	VkShaderModule shadowVert;
	if (!loadShaderModule("shaders/shadow_vert.spv", &shadowVert)) {
		throw std::runtime_error("failed to load shadow vert shader!");
	}
	// Ϊʲô��m_globalSetLayout
	std::array<VkDescriptorSetLayout, 1> layouts{ m_globalSetLayout };
	VkPipelineLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	layoutInfo.setLayoutCount = 1;
	layoutInfo.pSetLayouts = layouts.data();

	VkPushConstantRange pushConstantRange{};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(MeshPushConstants);

	layoutInfo.pushConstantRangeCount = 1;
	layoutInfo.pPushConstantRanges = &pushConstantRange;


	if (vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_shadowPipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create shadow pipeline layout!");
	}

	PipelineBuilder builder;
	builder.m_shaderStages.push_back({ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, shadowVert, "main" });

	auto bindingDescription = Vertex::getBindingDescription();
	auto attributeDescriptions = Vertex::getAttributeDescriptions();

	builder.m_vertexBindings.push_back(bindingDescription);
	builder.m_vertexAttributes = attributeDescriptions;
	builder.m_vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	builder.m_inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	builder.m_inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	builder.m_inputAssembly.primitiveRestartEnable = VK_FALSE;

	builder.m_dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	builder.m_viewport = { 0.0f,0.0f,(float)m_swapchainExtent.width,(float)m_swapchainExtent.height,0.0f,1.0f };
	builder.m_scissor = { {0,0},m_swapchainExtent };

	builder.m_depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	builder.m_depthStencil.depthTestEnable = VK_TRUE;
	builder.m_depthStencil.depthWriteEnable = VK_TRUE;
	builder.m_depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

	builder.m_rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	builder.m_rasterizer.depthClampEnable = VK_FALSE;
	builder.m_rasterizer.rasterizerDiscardEnable = VK_FALSE;
	builder.m_rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	builder.m_rasterizer.cullMode = VK_CULL_MODE_NONE; 
	builder.m_rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	builder.m_rasterizer.lineWidth = 1.0f;
	builder.m_rasterizer.depthBiasEnable = VK_TRUE;
	builder.m_rasterizer.depthBiasConstantFactor = 4.0f;
	builder.m_rasterizer.depthBiasSlopeFactor = 1.5f;

	builder.m_multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	builder.m_multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	builder.m_colorBlendAttachments.clear();

	builder.m_pipelineLayout = m_shadowPipelineLayout;
	m_shadowPipeline = builder.buildPipeline(m_device, m_shadowRenderPass);

	vkDestroyShaderModule(m_device, shadowVert, nullptr);

	m_mainDeletionQueue.push_function([=]() {
		vkDestroyPipelineLayout(m_device, m_shadowPipelineLayout, nullptr);
		vkDestroyPipeline(m_device, m_shadowPipeline, nullptr);
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
	// ��ʽ����֡����
	if (m_viewportFramebuffer != VK_NULL_HANDLE) {
		vkDestroyFramebuffer(m_device, m_viewportFramebuffer, nullptr);
		m_viewportFramebuffer = VK_NULL_HANDLE;
	}
	for (auto fb : m_frameBuffers) {
		vkDestroyFramebuffer(m_device, fb, nullptr);
	}
	m_frameBuffers.clear();

	// �����ӿ���ͼ��Դ
	if (m_viewportImage.imageView != VK_NULL_HANDLE) {
		vkDestroyImageView(m_device, m_viewportImage.imageView, nullptr);
		m_viewportImage.imageView = VK_NULL_HANDLE;
	}
	if (m_viewportImage.image != VK_NULL_HANDLE) {
		vmaDestroyImage(m_allocator, m_viewportImage.image, m_viewportImage.allocation);
		m_viewportImage.image = VK_NULL_HANDLE;
	}


	// ��ʽ���������Դ
	if (m_depthImage.imageView != VK_NULL_HANDLE) {
		vkDestroyImageView(m_device, m_depthImage.imageView, nullptr);
		m_depthImage.imageView = VK_NULL_HANDLE;
	}
	if (m_depthImage.image != VK_NULL_HANDLE) {
		vmaDestroyImage(m_allocator, m_depthImage.image, m_depthImage.allocation);
		m_depthImage.image = VK_NULL_HANDLE;
	}

	// ���پɵ�ͼ����ͼ
	for (auto view : m_swapchainImageViews) {
		vkDestroyImageView(m_device, view, nullptr);
	}
	m_swapchainImageViews.clear();

	if (m_swapchain != VK_NULL_HANDLE) {
		vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
		m_swapchain = VK_NULL_HANDLE;
	}

	// GBuffer
	if (m_gbufferFramebuffer != VK_NULL_HANDLE) {
		vkDestroyFramebuffer(m_device, m_gbufferFramebuffer, nullptr);
		m_gbufferFramebuffer = VK_NULL_HANDLE;
	}
	vkDestroyImageView(m_device, m_gbuffer.position.imageView, nullptr);
	vmaDestroyImage(m_allocator, m_gbuffer.position.image, m_gbuffer.position.allocation);
	vkDestroyImageView(m_device, m_gbuffer.normal.imageView, nullptr);
	vmaDestroyImage(m_allocator, m_gbuffer.normal.image, m_gbuffer.normal.allocation);
	vkDestroyImageView(m_device, m_gbuffer.albedo.imageView, nullptr);
	vmaDestroyImage(m_allocator, m_gbuffer.albedo.image, m_gbuffer.albedo.allocation);
	m_gbuffer = {};

	// Shadow
	vkDestroyFramebuffer(m_device, m_shadowFrameBuffer, nullptr);
	m_shadowFrameBuffer = VK_NULL_HANDLE;
	vkDestroyRenderPass(m_device, m_shadowRenderPass, nullptr);
	m_shadowRenderPass = VK_NULL_HANDLE;
	vkDestroyImageView(m_device, m_shadowImage.imageView, nullptr);
	vmaDestroyImage(m_allocator, m_shadowImage.image, m_shadowImage.allocation);
	m_shadowImage = {};
}

void ErisEngine::initVulkan()
{
	// 
    // 1. ���� Instance
	createInstance();

	// 2. ���� Surface
	createSurface();

	// 3. ��ѡ�����豸 (GPU)
	pickPhysicalDevice();

	// 4. �����߼��豸�ͻ�ȡ����
	// ����ͼ�ζ�����
	createLogicalDevice();


}


void ErisEngine::initVma()
{
	// 1. ��ʽ���� Vulkan ����ָ��
	VmaVulkanFunctions vulkanFunctions = {};
	vulkanFunctions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
	vulkanFunctions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;
	// �����ʹ�õ��� Vulkan 1.1+�����鲹ȫ��Щ����ѡ���Ƽ���
	vulkanFunctions.vkGetPhysicalDeviceMemoryProperties = &vkGetPhysicalDeviceMemoryProperties;
	vulkanFunctions.vkGetDeviceBufferMemoryRequirements = &vkGetDeviceBufferMemoryRequirements;
	vulkanFunctions.vkGetDeviceImageMemoryRequirements = &vkGetDeviceImageMemoryRequirements;

	VmaAllocatorCreateInfo allocatorInfo{};
	allocatorInfo.physicalDevice = m_physicalDevice;
	allocatorInfo.device = m_device;
	allocatorInfo.instance = m_instance;
	allocatorInfo.pVulkanFunctions = &vulkanFunctions; // ���ģ����뺯��ָ���

	// ��Ӧ�� Instance ��� apiVersion
	allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_4;

	if (vmaCreateAllocator(&allocatorInfo, &m_allocator) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create VMA Allocator!");
	}

	m_mainDeletionQueue.push_function([=]() {
		vmaDestroyAllocator(m_allocator);
		});
}

void ErisEngine::initSwapchain()
{
	// �˴�Ӧ�������� VkSwapchainKHR �Ĵ���
	// ��ֵ�� m_swapchain, m_swapchainImageFormat, m_swapchainImages, m_swapchainImageViews

	createSwapchain();
	createImageViews();
}

void ErisEngine::initCommands()
{
	// �˴�Ӧ�������� VkCommandPool ���߼�
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		// ����ÿ֡�������
		VkCommandPoolCreateInfo commandPoolInfo{};
		commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		commandPoolInfo.queueFamilyIndex = m_graphicsQueueFamily;
		commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		if (vkCreateCommandPool(m_device, &commandPoolInfo, nullptr, &m_frames[i].m_commandPool) != VK_SUCCESS) {
			throw std::runtime_error("failed to create per-frame command pool!");
		}

		// ����ÿ֡���������
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = m_frames[i].m_commandPool;
		allocInfo.commandBufferCount = 1;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		vkAllocateCommandBuffers(m_device, &allocInfo, &m_frames[i].m_mainCommandBuffer);

		// ע������
		m_mainDeletionQueue.push_function([=]() {
			vkDestroyCommandPool(m_device, m_frames[i].m_commandPool, nullptr);
			});
	}

}



void ErisEngine::initSyncStructures()
{
	// �˴�Ӧ�������� VkFence �� VkSemaphore ���߼�

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	// ��ʼ״̬��Ϊ Signaled��ȷ����һ֡�������޵ȴ�
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
	auto createAttachment = [](VkFormat format, VkImageLayout finalLayout) {
		VkAttachmentDescription att{};
		att.format = format;
		att.samples = VK_SAMPLE_COUNT_1_BIT;
		att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		att.finalLayout = finalLayout;
		return att;
		};

	// 1. ���� 4 ������ (3 ����ɫ��1 �����)
	std::array<VkAttachmentDescription, 4> attachments = {
		createAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL), // Position
		createAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL), // Normal
		createAttachment(VK_FORMAT_R8G8B8A8_UNORM,      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL), // Albedo
		createAttachment(m_depthFormat,                 VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) // Depth
	};

	// 2. ���帽������
	std::array<VkAttachmentReference, 3> colorRefs = {
		VkAttachmentReference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
		VkAttachmentReference{1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
		VkAttachmentReference{2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}
	};
	VkAttachmentReference depthRef = { 3, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };


	// 3. ��ͨ������ (�󶨶����ɫĿ��)
	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = static_cast<uint32_t>(colorRefs.size());
	subpass.pColorAttachments = colorRefs.data();
	subpass.pDepthStencilAttachment = &depthRef;
	
	// 4. ������ϵ (��֤��һ֡д����ٶ�)
	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	
	// 5. ���� RenderPass
	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS) {
		throw std::runtime_error("failed to create gbuffer render pass!");
	}

	m_mainDeletionQueue.push_function([=]() {
		vkDestroyRenderPass(m_device, m_renderPass, nullptr);
		});
}


void ErisEngine::initUIRenderPass() {
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = m_swapchainImageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // UI ��Ҳ���һ��
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // ���ո���ʾ��

	// 2. ��ȸ��� (Attachment 1) - ��ʹ UI ���ã�Ҳ���������Ա��ּ���
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = m_depthFormat; // ȷ��ʹ�ú����� Pass һ������ȸ�ʽ
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;  // UI �������������
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

void ErisEngine::initViewportPass()
{
	// 1. ��ɫ���� (���պϳɺ�Ļ���)
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = m_swapchainImageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// 2. ��ȸ���
	VkAttachmentDescription depthAttachment{};
	depthAttachment.format = m_depthFormat;
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorRef{};
	colorRef.attachment = 0;
	colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthRef{};
	depthRef.attachment = 1;
	depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorRef;
	subpass.pDepthStencilAttachment = &depthRef;

	VkRenderPassCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
	createInfo.attachmentCount = 2;
	createInfo.pAttachments = attachments.data();
	createInfo.subpassCount = 1;
	createInfo.pSubpasses = &subpass;

	vkCreateRenderPass(m_device, &createInfo, nullptr, &m_viewportPass);

	m_mainDeletionQueue.push_function([=]() {
		vkDestroyRenderPass(m_device, m_viewportPass, nullptr);
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
	// 1. ȷ����ʽ�ͳߴ�
	m_depthFormat = findDepthFormat();

	VkExtent3D depthImageExtent = {
		m_swapchainExtent.width,
		m_swapchainExtent.height,
		1
	};

	// 2. ����ͼ����Ϣ (VkImageCreateInfo)
	VkImageCreateInfo depthImageInfo{};
	depthImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	depthImageInfo.imageType = VK_IMAGE_TYPE_2D;
	depthImageInfo.format = m_depthFormat;
	depthImageInfo.extent = depthImageExtent;
	depthImageInfo.mipLevels = 1;
	depthImageInfo.arrayLayers = 1;
	depthImageInfo.samples = msaaSamples;
	depthImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	depthImageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

	// 3. ���� VMA �ڴ������Ϣ (VmaAllocationCreateInfo)
	// ��ȡ���˽̳��е� VkMemoryAllocateInfo
	VmaAllocationCreateInfo depthAllocInfo{};	
	depthAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;	// ���� VMA ����ڴ����Դ���
	depthAllocInfo.requiredFlags = (VkMemoryPropertyFlags)VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	// 4. ʹ�� VMA һ���Դ��� Image �� Allocation
   // ��ȡ���˽̳��е� vkCreateImage �� vkAllocateMemory
	VkResult res = vmaCreateImage(m_allocator, &depthImageInfo, &depthAllocInfo,
		&m_depthImage.image,
		&m_depthImage.allocation,
		nullptr);

	if (res != VK_SUCCESS) {
		throw std::runtime_error("failed to create depth image! Result: " + std::to_string(res));
	}

	// 5. ���ò�����ͼ����ͼ (VkImageViewCreateInfo)
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
	depthImageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT; // �ؼ������Ϊ����ӽ�

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

// ��ʼ����֤����Ϣ
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

// ����֧��ͼ������Ķ���
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
	appInfo.apiVersion = VK_API_VERSION_1_4;


	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	auto extensions = getRequiredExtensions();
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();

	// ������ʹ
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
	// 1. ������ʱ�������
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = m_frames[0].m_commandPool; // ���õ�0֡�ĳ�
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer cmd;
	vkAllocateCommandBuffers(m_device, &allocInfo, &cmd);

	// 2. ��ʼ¼��
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(cmd, &beginInfo);

	// 3. ִ���ⲿ����� Lambda �߼������翽�����壩
	function(cmd);

	// 4. �������ύ
	vkEndCommandBuffer(cmd);

	VkSubmitInfo submit{};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &cmd;

	vkQueueSubmit(m_graphicsQueue, 1, &submit, VK_NULL_HANDLE);
	vkQueueWaitIdle(m_graphicsQueue); // �ȴ��������

	// 5. �ͷ���ʱ����
	vkFreeCommandBuffers(m_device, m_frames[0].m_commandPool, 1, &cmd);
}

void ErisEngine::initDescriptors() {
	// ---------------------------------------------------------
	// 1. ����ȫ���Լ���Ӱ���� (Set 0) - ���� GPUSceneData
	// ---------------------------------------------------------
	VkDescriptorSetLayoutBinding sceneBinds[2];

    // Binding 0: ȫ�� UBO ����
	sceneBinds[0].binding = 0;
	sceneBinds[0].descriptorCount = 1;
	sceneBinds[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	sceneBinds[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

	// Binding 1: ��Ӱ�����ͼ (Sampler)
	sceneBinds[1].binding = 1;
	sceneBinds[1].descriptorCount = 1;
	sceneBinds[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	sceneBinds[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	sceneBinds[1].pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo globalLayoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	globalLayoutInfo.bindingCount = 2;
	globalLayoutInfo.pBindings = sceneBinds;
	if (vkCreateDescriptorSetLayout(m_device, &globalLayoutInfo, nullptr, &m_globalSetLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create global descriptor set layout!");
	}

	// ---------------------------------------------------------
	// 2.1 �������ʲ��� (Set 1) - ������ͼ
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
	// 2.2 ������պв��� (Set 1) - ������ͼ
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

	// 3. ���������� (Sampler)
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
	// �ؼ�����ֹ��Ե�ӷ�
	skySamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	skySamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	skySamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	if (vkCreateSampler(m_device, &skySamplerInfo, nullptr, &m_skyboxSampler) != VK_SUCCESS) {
		throw std::runtime_error("failed to create skybox sampler!");
	}

	// Lumen part
	std::vector<VkDescriptorSetLayoutBinding> lumenBinds;
	// Binding 0: Position + Emission (Sampler2D)
	lumenBinds.push_back({ 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr });
	// Binding 1: Normal + Metallic (Sampler2D)
	lumenBinds.push_back({ 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr });
	// Binding 2: Albedo + Roughness (Sampler2D)
	lumenBinds.push_back({ 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr });

	VkDescriptorSetLayoutBinding skyBind{};
	skyBind.binding = 3;
	skyBind.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	skyBind.descriptorCount = 1;
	skyBind.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	skyBind.pImmutableSamplers = nullptr;
	lumenBinds.push_back(skyBind);


	VkDescriptorSetLayoutCreateInfo lumenLayoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	lumenLayoutInfo.bindingCount = static_cast<uint32_t>(lumenBinds.size());
	lumenLayoutInfo.pBindings = lumenBinds.data();
	vkCreateDescriptorSetLayout(m_device, &lumenLayoutInfo, nullptr, &m_lumenDescriptorLayout);


	// ע������ (ע��˳����ɾ Pool ��ɾ Layout)
	m_mainDeletionQueue.push_function([=]() {
		vkDestroySampler(m_device, m_sampler, nullptr);
		vkDestroySampler(m_device, m_skyboxSampler, nullptr);
		vkDestroyDescriptorSetLayout(m_device, m_globalSetLayout, nullptr);
		vkDestroyDescriptorSetLayout(m_device, m_singleTextureLayout, nullptr);
		vkDestroyDescriptorSetLayout(m_device, m_skyboxDescriptorSetLayout, nullptr);
		vkDestroyDescriptorSetLayout(m_device, m_lumenDescriptorLayout, nullptr);
		});
}

void ErisEngine::initDescriptorPool()
{
	std::vector<VkDescriptorPoolSize>sizes = {
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,100},			// ֧�� 100 ��ȫ�����ݿ�
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,500}		// ֧�� 500 �Ų�����ͼ
	};

	VkDescriptorPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;						// ���������ͷ�
	poolInfo.maxSets = 600;																	// �ܼ���
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
	imageBufferInfo.sampler = m_sampler; // ȷ�����Ѿ��� initDescriptors �ﴴ������
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

void ErisEngine::initGbuffer()
{
	// �������ź���Ļһ�������ͼ

	VkExtent3D extent = { m_swapchainExtent.width, m_swapchainExtent.height, 1 };

	// 1. λ�û��� (��Ҫ�߾��ȣ��� 16 λ������)
	VkImageCreateInfo posInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	posInfo.imageType = VK_IMAGE_TYPE_2D;
	posInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	posInfo.extent = extent;
	posInfo.mipLevels = 1;
	posInfo.arrayLayers = 1;
	posInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	posInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	posInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

	VmaAllocationCreateInfo allocInfo{ VMA_MEMORY_USAGE_GPU_ONLY };
	vmaCreateImage(m_allocator, &posInfo, &allocInfo, &m_gbuffer.position.image, &m_gbuffer.position.allocation, nullptr);
	m_gbuffer.position.imageView = createImageView(m_gbuffer.position.image, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 1);

	// 2. ���߻��� (ͬ����Ҫ�߾��ȴ淨�߷�����ӽ�����)
	VkImageCreateInfo normInfo = posInfo;
	vmaCreateImage(m_allocator, &normInfo, &allocInfo, &m_gbuffer.normal.image, &m_gbuffer.normal.allocation, nullptr);
	m_gbuffer.normal.imageView = createImageView(m_gbuffer.normal.image, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 1);

	// 3. ��ɫ/�ֲڶȻ��� (8λ�͹���)
	VkImageCreateInfo albedoInfo = posInfo;
	albedoInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	vmaCreateImage(m_allocator, &albedoInfo, &allocInfo, &m_gbuffer.albedo.image, &m_gbuffer.albedo.allocation, nullptr);
	m_gbuffer.albedo.imageView = createImageView(m_gbuffer.albedo.image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1);


	std::array<VkImageView, 4> attachments = {
		m_gbuffer.position.imageView,
		m_gbuffer.normal.imageView,
		m_gbuffer.albedo.imageView,
		m_depthImage.imageView
	};

	VkFramebufferCreateInfo fbInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
	fbInfo.renderPass = m_renderPass;
	fbInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	fbInfo.pAttachments = attachments.data();
	fbInfo.width = m_swapchainExtent.width;
	fbInfo.height = m_swapchainExtent.height;
	fbInfo.layers = 1;
	if (vkCreateFramebuffer(m_device, &fbInfo, nullptr, &m_gbufferFramebuffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to create GBuffer framebuffer!");
	}

	// ע������
	m_mainDeletionQueue.push_function([=]() {
		vkDestroyFramebuffer(m_device, m_gbufferFramebuffer, nullptr);
		vkDestroyImageView(m_device, m_gbuffer.position.imageView, nullptr);
		vmaDestroyImage(m_allocator, m_gbuffer.position.image, m_gbuffer.position.allocation);
										
		vkDestroyImageView(m_device, m_gbuffer.normal.imageView, nullptr);
		vmaDestroyImage(m_allocator, m_gbuffer.normal.image, m_gbuffer.normal.allocation);
										
		vkDestroyImageView(m_device, m_gbuffer.albedo.imageView, nullptr);
		vmaDestroyImage(m_allocator, m_gbuffer.albedo.image, m_gbuffer.albedo.allocation);
		});

}

void ErisEngine::updateLumenDescriptorSet()
{
	VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	allocInfo.descriptorPool = m_descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &m_lumenDescriptorLayout;
	vkAllocateDescriptorSets(m_device, &allocInfo, &m_lumenDescriptorSet);

	VkDescriptorImageInfo posInfo{};
	posInfo.sampler = m_sampler;
	posInfo.imageView = m_gbuffer.position.imageView;
	posInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkDescriptorImageInfo normInfo{};
	normInfo.sampler = m_sampler;
	normInfo.imageView = m_gbuffer.normal.imageView;
	normInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkDescriptorImageInfo albInfo{};
	albInfo.sampler = m_sampler;
	albInfo.imageView = m_gbuffer.albedo.imageView;
	albInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkDescriptorImageInfo skyInfo{};
	skyInfo.sampler = m_skyboxSampler; // ʹ�����ʼ���õ���պв�����
	skyInfo.imageView = m_skyboxImage.imageView;
	skyInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	std::array<VkWriteDescriptorSet, 4> writes{};

	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = m_lumenDescriptorSet;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[0].pImageInfo = &posInfo;

	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = m_lumenDescriptorSet;
	writes[1].dstBinding = 1;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[1].pImageInfo = &normInfo;

	writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[2].dstSet = m_lumenDescriptorSet;
	writes[2].dstBinding = 2;
	writes[2].descriptorCount = 1;
	writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[2].pImageInfo = &albInfo;

	writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[3].dstSet = m_lumenDescriptorSet;
	writes[3].dstBinding = 3;
	writes[3].descriptorCount = 1;
	writes[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[3].pImageInfo = &skyInfo;

	vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void ErisEngine::initLumenLightingPipeline()
{
	// �������Ǹ�д�� lumen_lighting.vert �� lumen_main.frag
	VkShaderModule vertMod, fragMod;
	if (!loadShaderModule("shaders/Lumen/main_vert.spv", &vertMod) || !loadShaderModule("shaders/Lumen/main_frag.spv", &fragMod)) {
		throw std::runtime_error("failed to load lumen light shader module!");
	}

	// ������ƣ�Set 0 ��ȫ������(SceneData), Set 1 �� GBuffer
	std::array<VkDescriptorSetLayout, 2> layouts = { m_globalSetLayout, m_lumenDescriptorLayout };

	VkPipelineLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	layoutInfo.setLayoutCount = static_cast<uint32_t>(layouts.size());
	layoutInfo.pSetLayouts = layouts.data();
	vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_lumenLightingPipelineLayout);

	PipelineBuilder builder;
	builder.m_shaderStages.push_back({ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertMod, "main" });
	builder.m_shaderStages.push_back({ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragMod, "main" });

	// ���ؼ��������ﲻ��Ҫ�κζ�������
	builder.m_vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	builder.m_vertexBindings.clear();
	builder.m_vertexAttributes.clear();

	builder.m_inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	builder.m_inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	builder.m_inputAssembly.primitiveRestartEnable = VK_FALSE;

	builder.m_viewport.x = 0.0f;
	builder.m_viewport.y = 0.0f;
	builder.m_viewport.width = static_cast<float>(m_swapchainExtent.width);
	builder.m_viewport.height = static_cast<float>(m_swapchainExtent.height);
	builder.m_viewport.minDepth = 0.0f;
	builder.m_viewport.maxDepth = 1.0f;

	builder.m_scissor.offset.x = 0;
	builder.m_scissor.offset.y = 0;
	builder.m_scissor.extent.width = m_swapchainExtent.width;
	builder.m_scissor.extent.height = m_swapchainExtent.height;

	builder.m_dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

	builder.m_rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	builder.m_rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	builder.m_rasterizer.cullMode = VK_CULL_MODE_NONE;
	builder.m_rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	builder.m_rasterizer.lineWidth = 1.0f;

	builder.m_multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	builder.m_multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	builder.m_depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	builder.m_depthStencil.depthTestEnable = VK_FALSE;
	builder.m_depthStencil.depthWriteEnable = VK_FALSE;
	builder.m_depthStencil.depthCompareOp = VK_COMPARE_OP_ALWAYS;


	// ��ɫ��� (����������ӿ�ͼ)
	VkPipelineColorBlendAttachmentState blend{};
	blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	blend.blendEnable = VK_FALSE;
	builder.m_colorBlendAttachments.clear();
	builder.m_colorBlendAttachments.push_back(blend);


	builder.m_pipelineLayout = m_lumenLightingPipelineLayout;
	m_lumenLightingPipeline = builder.buildPipeline(m_device, m_viewportPass); 

	vkDestroyShaderModule(m_device, vertMod, nullptr);
	vkDestroyShaderModule(m_device, fragMod, nullptr);

	m_mainDeletionQueue.push_function([=]() {
		vkDestroyPipelineLayout(m_device, m_lumenLightingPipelineLayout, nullptr);
		vkDestroyPipeline(m_device, m_lumenLightingPipeline, nullptr);
		});

}

void ErisEngine::initLumenGbufferPipeline()
{
	VkShaderModule vertMod, fragMod;
	if (!loadShaderModule("shaders/Lumen/gbuffer_vert.spv", &vertMod) || !loadShaderModule("shaders/Lumen/gbuffer_frag.spv", &fragMod)) {
		throw std::runtime_error("failed to load lumen gbuffer shader module!");
	}
	std::array<VkDescriptorSetLayout, 2> layouts = { m_globalSetLayout, m_singleTextureLayout };

	VkPushConstantRange pushRange{};
	pushRange.offset = 0;
	pushRange.size = sizeof(MeshPushConstants);
	pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

	VkPipelineLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	layoutInfo.setLayoutCount = static_cast<uint32_t>(layouts.size());
	layoutInfo.pSetLayouts = layouts.data();
	layoutInfo.pushConstantRangeCount = 1;
	layoutInfo.pPushConstantRanges = &pushRange;
	vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_lumenGbufferPipelineLayout);

	// 2. ��������
	PipelineBuilder builder;
	builder.m_shaderStages.push_back({ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertMod, "main" });
	builder.m_shaderStages.push_back({ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragMod, "main" });

	builder.m_vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	builder.m_vertexBindings.push_back(Vertex::getBindingDescription());
	builder.m_vertexAttributes = Vertex::getAttributeDescriptions();

	builder.m_inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	builder.m_inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	builder.m_inputAssembly.primitiveRestartEnable = VK_FALSE;

	builder.m_viewport.x = 0.0f;
	builder.m_viewport.y = 0.0f;
	builder.m_viewport.width = static_cast<float>(m_swapchainExtent.width);
	builder.m_viewport.height = static_cast<float>(m_swapchainExtent.height);
	builder.m_viewport.minDepth = 0.0f;
	builder.m_viewport.maxDepth = 1.0f;

	builder.m_scissor.offset.x = 0;
	builder.m_scissor.offset.y = 0;
	builder.m_scissor.extent.width = m_swapchainExtent.width;
	builder.m_scissor.extent.height = m_swapchainExtent.height;

	builder.m_dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

	builder.m_rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	builder.m_rasterizer.depthClampEnable = VK_FALSE;
	builder.m_rasterizer.rasterizerDiscardEnable = VK_FALSE;
	builder.m_rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	builder.m_rasterizer.cullMode = VK_CULL_MODE_NONE;
	builder.m_rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	builder.m_rasterizer.depthBiasEnable = VK_FALSE;
	builder.m_rasterizer.lineWidth = 1.0f;

	builder.m_multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	builder.m_multisampling.sampleShadingEnable = VK_FALSE;
	builder.m_multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	builder.m_depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	builder.m_depthStencil.depthTestEnable = VK_TRUE;
	builder.m_depthStencil.depthWriteEnable = VK_TRUE;
	builder.m_depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	builder.m_depthStencil.depthBoundsTestEnable = VK_FALSE;
	builder.m_depthStencil.stencilTestEnable = VK_FALSE;

	// ���� 3 ����ϸ��� (MRT ����ƥ��)
	VkPipelineColorBlendAttachmentState opaqueBlend{};
	opaqueBlend.blendEnable = VK_FALSE;
	opaqueBlend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	builder.m_colorBlendAttachments.clear();
	builder.m_colorBlendAttachments.push_back(opaqueBlend); // Location 0: Position
	builder.m_colorBlendAttachments.push_back(opaqueBlend); // Location 1: Normal
	builder.m_colorBlendAttachments.push_back(opaqueBlend); // Location 2: Albedo


	builder.m_pipelineLayout = m_lumenGbufferPipelineLayout;
	builder.m_depthStencil = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, nullptr, 0, VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL, VK_FALSE, VK_FALSE, {}, {}, 0.0f, 1.0f };

	builder.m_depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	builder.m_depthStencil.pNext = nullptr;
	builder.m_depthStencil.flags = 0;
	builder.m_depthStencil.depthTestEnable = VK_TRUE;
	builder.m_depthStencil.depthWriteEnable = VK_TRUE;
	builder.m_depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	builder.m_depthStencil.depthBoundsTestEnable = VK_FALSE;
	builder.m_depthStencil.stencilTestEnable = VK_FALSE;
	builder.m_depthStencil.minDepthBounds = 0.0f;
	builder.m_depthStencil.maxDepthBounds = 1.0f;

	// �󶨵��Ǹ�֧�� 4 ������3ɫ1��� RenderPass
	m_lumenGbufferPipeline = builder.buildPipeline(m_device, m_renderPass);

	vkDestroyShaderModule(m_device, vertMod, nullptr);
	vkDestroyShaderModule(m_device, fragMod, nullptr);

	m_mainDeletionQueue.push_function([=]() {
		vkDestroyPipelineLayout(m_device, m_lumenGbufferPipelineLayout, nullptr);
		vkDestroyPipeline(m_device, m_lumenGbufferPipeline, nullptr);
		});

}
void ErisEngine::drawForwardFrame(VkCommandBuffer cmd, FrameData& frame, uint32_t imageIndex)
{
	// 1. ��Ⱦ��Ӱ��ͼ (���� Lumen ·���ĺ���)
	executeShadowPass(cmd);

	// 2. ��Ⱦ������ (Forward ·��ר��)
	executeForwardPass(cmd);

	// 3. ���Ʊ༭�� UI (���� Lumen ·���ĺ���)
	executeEditorUIPass(cmd, imageIndex);
}
void ErisEngine::executeShadowPass(VkCommandBuffer cmd) {
	// --- 1. ��ʽ���� RenderPass ��ʼ��Ϣ ---
	VkRenderPassBeginInfo shadowRpInfo{};
	shadowRpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	shadowRpInfo.pNext = nullptr;
	shadowRpInfo.renderPass = m_shadowRenderPass;
	shadowRpInfo.framebuffer = m_shadowFrameBuffer;
	shadowRpInfo.renderArea.offset.x = 0;
	shadowRpInfo.renderArea.offset.y = 0;
	shadowRpInfo.renderArea.extent.width = m_shadowExtent.width;
	shadowRpInfo.renderArea.extent.height = m_shadowExtent.height;

	VkClearValue shadowClear{};
	shadowClear.depthStencil.depth = 1.0f;
	shadowClear.depthStencil.stencil = 0;

	shadowRpInfo.clearValueCount = 1;
	shadowRpInfo.pClearValues = &shadowClear;

	// --- 2. ִ�л��� ---
	vkCmdBeginRenderPass(cmd, &shadowRpInfo, VK_SUBPASS_CONTENTS_INLINE);
		drawShadow(cmd); 
	vkCmdEndRenderPass(cmd);

	// --- 3. ��ʽͬ������Ӱд���תΪ Shader �ɶ� ---
	transitionImageLayout(cmd, m_shadowImage.image, m_depthFormat,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1, 0);
}

void ErisEngine::executeForwardPass(VkCommandBuffer cmd) {
	// --- 1. ͬ�������ӿ�ͼ�ӡ���ȡ��תΪ��д�롱״̬ ---
	transitionImageLayout(cmd, m_viewportImage.image, m_swapchainImageFormat,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1,1,0);

	// --- 2. �������ֵ (1������ɫ + 1�����) ---
	std::array<VkClearValue, 2> clears{};
	// ����ɫ������ɫ
	clears[0].color.float32[0] = 0.03f;
	clears[0].color.float32[1] = 0.03f;
	clears[0].color.float32[2] = 0.03f;
	clears[0].color.float32[3] = 1.0f;
	// ���
	clears[1].depthStencil.depth = 1.0f;
	clears[1].depthStencil.stencil = 0;

	// --- 3. ���� RenderPass ��ʼ��Ϣ ---
	VkRenderPassBeginInfo sceneRpInfo{};
	sceneRpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	sceneRpInfo.pNext = nullptr;
	sceneRpInfo.renderPass = m_viewportPass; // ʹ�õ������ϳ� Pass
	sceneRpInfo.framebuffer = m_viewportFramebuffer;
	sceneRpInfo.renderArea.offset.x = 0;
	sceneRpInfo.renderArea.offset.y = 0;
	sceneRpInfo.renderArea.extent.width = m_swapchainExtent.width;
	sceneRpInfo.renderArea.extent.height = m_swapchainExtent.height;
	sceneRpInfo.clearValueCount = static_cast<uint32_t>(clears.size());
	sceneRpInfo.pClearValues = clears.data();

	// --- 4. ִ�л������� ---
	vkCmdBeginRenderPass(cmd, &sceneRpInfo, VK_SUBPASS_CONTENTS_INLINE);

		// A. ��ģ�� (ʹ�� Forward ר�ù���)
		drawMainGeometry(cmd, m_pipeline, m_pipelineLayout);

		// B. ����պ�
		drawSkybox(cmd);

		// C. ������
		drawGrid(cmd);

	vkCmdEndRenderPass(cmd);

	// --- 5. ͬ������Ⱦ�������Զ�ת�� SHADER_READ_ONLY �� ImGui ʹ�� ---
	// (��Ϊ m_viewportPass �� finalLayout �Ѿ����ú��ˣ��˴������ֶ�ת��)
	transitionImageLayout(cmd, m_depthImage.image, m_depthFormat,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, 1,1,0);
}

void ErisEngine::drawLumenFrame(VkCommandBuffer cmd,FrameData& frame, uint32_t imageIndex)
{
	// ---------------------------------------------------------
	// ��PASS 0: ��Ӱ��Ⱦ��
	// ---------------------------------------------------------
	executeShadowPass(cmd);

	// ---------------------------------------------------------
	// ��PASS 1: Geometry Pass - ��� GBuffer��
	// ---------------------------------------------------------
	executeGBufferPass(cmd);
	transitionImageLayout(cmd, m_gbuffer.position.image, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,1,0);
	transitionImageLayout(cmd, m_gbuffer.normal.image, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,1,0);
	transitionImageLayout(cmd, m_gbuffer.albedo.image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,1,0);
	// ---------------------------------------------------------
	// ��PASS 2: Lighting Pass - ���չ����� Lumen ���㡿
	// ---------------------------------------------------------
	executeLumenCompositionPass(cmd);

	// ---------------------------------------------------------
   // ��PASS 3: UI Pass - ���� ImGui ���浽���ս�������
   // ---------------------------------------------------------
	executeEditorUIPass(cmd, imageIndex);
	
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

	uint32_t imageIndex;
	VkResult result = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, frame.m_presentSemaphore, VK_NULL_HANDLE, &imageIndex);

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

	glm::vec3 lightPos = glm::vec3(m_sceneData.sunlightDir) * 40.0f;
	glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
	glm::mat4 lightProj = glm::ortho(-15.0f, 15.0f, -15.0f, 15.0f, 0.1f, 100.0f);
	lightProj[1][1] *= -1;

	// in memorial of this disater!!!!!
	glm::mat4 vulkanZ = glm::mat4(1.0f);
	vulkanZ[2][2] = 0.5f;
	vulkanZ[3][2] = 0.5f;

	m_sceneData.sunlightProj = vulkanZ * lightProj * lightView;
	m_sceneData.viewPos = glm::vec4(m_camera.m_position, 1.0f);


	// 2. ������д�뱾֡�� UBO �ڴ�
	void* data;
	vmaMapMemory(m_allocator, frame.sceneBuffer.allocation, &data);
		memcpy(data, &m_sceneData, sizeof(GPUSceneData));
	vmaUnmapMemory(m_allocator, frame.sceneBuffer.allocation);

	VkCommandBuffer cmd = frame.m_mainCommandBuffer;
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
		throw std::runtime_error("failed to begin recording command buffer!");
	}


	if (m_activePath == RenderPath::Forward){
		drawForwardFrame(cmd, frame, imageIndex);
	}
	else {
		drawLumenFrame(cmd, frame, imageIndex);
	}


	if (vkEndCommandBuffer(frame.m_mainCommandBuffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to record command buffer!");
	}

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &frame.m_presentSemaphore;	// �ȴ�ͼ���ȡ���
	submitInfo.pWaitDstStageMask = waitStages;

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &frame.m_mainCommandBuffer;

	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &frame.m_renderSemaphore;	// ������Ⱦ����ź�

	if (vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, frame.m_renderFence) != VK_SUCCESS) {
		throw std::runtime_error("failed to submit draw command buffer!");
	}

	// ����ͼ��
	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &frame.m_renderSemaphore; // �ȴ���Ⱦ���
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

void ErisEngine::transitionImageLayout(VkCommandBuffer cmd, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t layerCount, uint32_t mipLevels, uint32_t baseMipLevel) {
	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.pNext = nullptr;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;

	barrier.subresourceRange.baseMipLevel = baseMipLevel;
	barrier.subresourceRange.levelCount = mipLevels;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = layerCount;


	barrier.subresourceRange.aspectMask = GetAspectMask(format);

	auto srcInfo = GetSrcAccessForLayout(oldLayout);
	auto dstInfo = GetDstAccessForLayout(newLayout);
	barrier.srcAccessMask = srcInfo.access;
	barrier.dstAccessMask = dstInfo.access;

	vkCmdPipelineBarrier(cmd, srcInfo.stage, dstInfo.stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}


// ----------------------Handle External Events------------------------

void ErisEngine::handleInput(float deltaTime)
{
	// 1. �����ƶ��߼� (WASD)
	glm::vec3 moveDir(0.0f);
	// ǰ��
	if (glfwGetKey(m_window, GLFW_KEY_W) == GLFW_PRESS) moveDir += m_camera.m_front;
	if (glfwGetKey(m_window, GLFW_KEY_S) == GLFW_PRESS) moveDir -= m_camera.m_front;
	// ���� (ͨ����˼����ҷ�������)
	glm::vec3 right = glm::normalize(glm::cross(m_camera.m_front, m_camera.m_up));
	if (glfwGetKey(m_window, GLFW_KEY_A) == GLFW_PRESS) moveDir -= right;
	if (glfwGetKey(m_window, GLFW_KEY_D) == GLFW_PRESS) moveDir += right;

	if (glm::length(moveDir) > 0.0f) {
		m_camera.processKeyboard(glm::normalize(moveDir), deltaTime);
	}

	// 2. �����ת�߼� (ģ�� UE5����ס�Ҽ�ʱ��ת)
	if (glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
		if (!m_isMousePressed) {
			// ���ΰ���ʱ��¼λ�ã���ֹ�ӽ�����
			glfwGetCursorPos(m_window, &m_lastX, &m_lastY);
			m_isMousePressed = true;
			// ��༭��һ��������꣬����������ת
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
			// �ͷ��Ҽ���ָ������ʾ
			glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}
	}
}

void ErisEngine::initViewportResources()
{
	// 1. ��������ͼ�� (m_viewportImage)
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

	// 2. �����ӿ� Framebuffer
	// ע�⣺���������� ��ɫ����(�ӿ�ͼ) �� ��ȸ���(���ͼ)
	std::array<VkImageView, 2> attachments = { m_viewportImage.imageView, m_depthImage.imageView };

	VkFramebufferCreateInfo fbInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
	fbInfo.renderPass = m_viewportPass;
	fbInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	fbInfo.pAttachments = attachments.data();
	fbInfo.width = m_swapchainExtent.width;
	fbInfo.height = m_swapchainExtent.height;
	fbInfo.layers = 1;

	// �ؼ��㣺ȷ����ֵ�����Ա m_viewportFramebuffer
	if (vkCreateFramebuffer(m_device, &fbInfo, nullptr, &m_viewportFramebuffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to create viewport framebuffer!");
	}

	immediateSubmit([&](VkCommandBuffer cmd) {
		transitionImageLayout(cmd, m_viewportImage.image, m_swapchainImageFormat,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,1);
			transitionImageLayout(cmd, m_depthImage.image, m_depthFormat,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,1);
		}); 

	if (m_viewportTextureSet != VK_NULL_HANDLE) {
		ImGui_ImplVulkan_RemoveTexture(m_viewportTextureSet);
	}

	// 3. ��������ͼ���� ImGui ע��
	// ע�⣺������ع���������ImGui �ᴦ���ɵ���ͼ ID����������Ҫ�ֶ� RemoveTexture
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

		// ��ɫ (���ģ�ʹ�����ɫ������Ĭ�ϰ�ɫ)
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
	// �ؼ���PreTransformVertices �ı�㼶��FlipUVs ���� Vulkan
	const aiScene* scene = importer.ReadFile(filename,
		aiProcess_Triangulate |
		aiProcess_FlipUVs |
		aiProcess_JoinIdenticalVertices |
		aiProcess_PreTransformVertices |
		aiProcess_GenSmoothNormals); // ȷ�����û�з������Զ�����

	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
		std::cerr << "ASSIMP ERROR: " << importer.GetErrorString() << std::endl;
		return false;
	}

	// ��ȡ�ļ���·�����ڼ�����ͼ
	std::string directory = filename.substr(0, filename.find_last_of("\\/"));

	// std::cout << directory << std::endl;
	outModel.meshes.clear();
	outModel.materials.clear();

	// --- ��һ�����������в��� (MTL) ---
	// Ԥ����ռ䣬��ֹ vector ���ݵ���ָ��ʧЧ
	outModel.materials.reserve(scene->mNumMaterials);

	std::cout << scene->mNumMaterials << std::endl;

	for (uint32_t i = 0; i < scene->mNumMaterials; i++) {
		aiMaterial* aiMat = scene->mMaterials[i];
		Material engineMat;

		// 2. ������ͼ (map_Kd)
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
	// --- �ڶ����������������� (Mesh) ---
	for (unsigned int m = 0; m < scene->mNumMeshes; m++) {
		aiMesh* aiMeshPtr = scene->mMeshes[m];
		Mesh localMesh;

		// ��ȡ��Ӧ�Ĳ�����ɫ��Ϊ�����ɫ
		aiMaterial* aiMat = scene->mMaterials[aiMeshPtr->mMaterialIndex];
		aiColor3D kd(1.0f, 1.0f, 1.0f);
		aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, kd);


		// ��䶥��
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

			// ��ɫ��ʹ�� MTL �� Kd ��ɫ
			v.color = { kd.r, kd.g, kd.b };

			localMesh.vertices.push_back(v);
		}

		// �������
		for (uint32_t i = 0; i < aiMeshPtr->mNumFaces; i++) {
			aiFace face = aiMeshPtr->mFaces[i];
			for (uint32_t j = 0; j < face.mNumIndices; j++) {
				localMesh.indices.push_back(face.mIndices[j]);
			}
		}

		// ��������ָ��
		if (aiMeshPtr->mMaterialIndex < outModel.materials.size()) {
			localMesh.material = &outModel.materials[aiMeshPtr->mMaterialIndex];
		}

		outModel.minBound = glm::min(outModel.minBound, localMesh.minBound);
		outModel.maxBound = glm::max(outModel.maxBound, localMesh.maxBound);
		outModel.meshes.push_back(localMesh);
	}


	// --- ���������ϴ� GPU ---
	for (auto& m : outModel.meshes) {
		m.uploadMesh(m_allocator, this);
	}

	std::cout << "[Engine] Successfully loaded " << filename << " with " << outModel.meshes.size() << " parts." << std::endl;
	return true;
}

AllocatedImage ErisEngine::loadImageFromFile(const char* file)
{
	int texWidth, texHeight, texChannels;

	// 1. ʹ�� stb_image �����������ݣ�ǿ��Ϊ 4 ͨ�� RGBA��
	stbi_uc* pixels = stbi_load(file, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	if (!pixels) {
		std::cerr << "Failed to load texture file: " << file << std::endl;
		// ����һ���յľ������ֹ����
		return AllocatedImage{ VK_NULL_HANDLE, VK_NULL_HANDLE, nullptr };
	}

	VkDeviceSize imageSize = texWidth * texHeight * 4;
	VkFormat imageFormat = VK_FORMAT_R8G8B8A8_SRGB; // ��ɫ��ͼ����ʹ�� SRGB

	// 2. �����ݴ滺�� (Staging Buffer)
	VkBufferCreateInfo stagingInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	stagingInfo.size = imageSize;
	stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	VmaAllocationCreateInfo stagingAllocInfo{};
	stagingAllocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

	AllocatedBuffer stagingbuffer;
	vmaCreateBuffer(m_allocator, &stagingInfo, &stagingAllocInfo, &stagingbuffer.buffer, &stagingbuffer.allocation, nullptr);

	// �����������ݵ��ݴ滺��
	void* data;
	vmaMapMemory(m_allocator, stagingbuffer.allocation, &data);
	memcpy(data, pixels, static_cast<size_t>(imageSize));
	vmaUnmapMemory(m_allocator, stagingbuffer.allocation);
	stbi_image_free(pixels); // �ͷ��ڴ��е�����

	// 3. ���� GPU ͼ�� (VkImage)
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
	dimg_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT; // ���ܽ��տ��������ܱ� Shader ����

	VmaAllocationCreateInfo dimg_allocinfo{};
	dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	AllocatedImage newImage;
	vmaCreateImage(m_allocator, &dimg_info, &dimg_allocinfo, &newImage.image, &newImage.allocation, nullptr);

	// 4. �����ݴ� Buffer ������ Image (���� immediateSubmit)
	immediateSubmit([&](VkCommandBuffer cmd) {
		// A. ת�����֣�Undefined -> Transfer Destination (׼����������)
		transitionImageLayout(cmd, newImage.image, imageFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1);

		// B. ִ�п���
		VkBufferImageCopy copyRegion{};
		copyRegion.bufferOffset = 0;
		copyRegion.bufferRowLength = 0;
		copyRegion.bufferImageHeight = 0;
		copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.imageSubresource.mipLevel = 0;
		copyRegion.imageSubresource.baseArrayLayer = 0;
		copyRegion.imageSubresource.layerCount = 1;
		copyRegion.imageExtent = imageExtent;

		vkCmdCopyBufferToImage(cmd, stagingbuffer.buffer, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

		// C. ת�����֣�Transfer Destination -> Shader Read Only (׼���� Shader ��)
		transitionImageLayout(cmd, newImage.image, imageFormat, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,1);
		});

	// 5. ���� Image View
	newImage.imageView = createImageView(newImage.image, imageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);

	// 6. �����ݴ滺��
	vmaDestroyBuffer(m_allocator, stagingbuffer.buffer, stagingbuffer.allocation);

	return newImage;
}

void ErisEngine::initDefaultResources() {
	// 1. ����һ������ɫ���� (RGBA)
	uint32_t whitePixel = 0xFFFFFFFF;

	// 2. �����ݴ滺�� (Staging Buffer) - ����ȷ�� CPU �ɼ�
	VkBufferCreateInfo stagingInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	stagingInfo.size = 4;
	stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	VmaAllocationCreateInfo stagingAllocInfo{};
	// �����㣺�������ּ�С���ݴ滺�壬ʹ�� CPU_ONLY �����Ƚ��ģ���֤ HOST_VISIBLE �� HOST_COHERENT
	stagingAllocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

	AllocatedBuffer stagingbuffer;
	// ���ﲻ��Ҫ�� VmaAllocationInfo �ˣ���Ϊ���Ǻ����ֶ� Map
	if (vmaCreateBuffer(m_allocator, &stagingInfo, &stagingAllocInfo,
		&stagingbuffer.buffer, &stagingbuffer.allocation, nullptr) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create staging buffer for default texture");
	}

	// ��������
	void* data;
	vmaMapMemory(m_allocator, stagingbuffer.allocation, &data);
	memcpy(data, &whitePixel, 4);
	vmaUnmapMemory(m_allocator, stagingbuffer.allocation);

	// 3. ���� 1x1 GPU ͼ��
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
	gpuImageAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY; // ͼ�����ݽ� GPU ����

	if (vmaCreateImage(m_allocator, &imgInfo, &gpuImageAllocInfo,
		&m_defaultTexture.image, &m_defaultTexture.allocation, nullptr) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create 1x1 default image");
	}

	// 4. ����֮ǰд�õ� transitionImageLayout ���в���ת���Ϳ���
	immediateSubmit([&](VkCommandBuffer cmd) {
		// A. ת������Ϊ���ն�
		transitionImageLayout(cmd, m_defaultTexture.image, VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1);

		// B. ������������ͼ��
		VkBufferImageCopy copyRegion{};
		copyRegion.bufferOffset = 0;
		copyRegion.bufferRowLength = 0;
		copyRegion.bufferImageHeight = 0;
		copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.imageSubresource.mipLevel = 0;
		copyRegion.imageSubresource.baseArrayLayer = 0;
		copyRegion.imageSubresource.layerCount = 1;
		copyRegion.imageExtent = extent;

		vkCmdCopyBufferToImage(cmd, stagingbuffer.buffer, m_defaultTexture.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

		// C. ת��Ϊ Shader �ɶ�����
		transitionImageLayout(cmd, m_defaultTexture.image, VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1);
		});

	// 5. ������ͼ
	m_defaultTexture.imageView = createImageView(m_defaultTexture.image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1);

	// 6. ����Ĭ���������� (���������Ӧ���Ѿ�ʵ�ֺ���)
	m_defaultTextureSet = createDescriptorSet(m_defaultTexture);

	// 7. �����ݴ滺�� (��ʱ������������)
	vmaDestroyBuffer(m_allocator, stagingbuffer.buffer, stagingbuffer.allocation);

	// 8. ע�ᱣ����ͼ�������߼��� DeletionQueue
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
	m_sceneData.lightCount = 0; // ��ʼû�е��Դ
}



// ---------------------------World Functions---------------------------------


Model* ErisEngine::getOrLoadModel(const std::string& path) {
	// 1. ���ģ���Ѿ��ڳ������ˣ�ֱ�ӷ���ָ��
	if (m_assetLibrary.find(path) != m_assetLibrary.end()) {
		return &m_assetLibrary[path];
	}

	// 2. ������ Model ����
	Model& newModel = m_assetLibrary[path];

	// 3. ִ�������Ĵ��̼���
	if (loadModelFromFile(path, newModel)) {
		// 4. �ϴ��� GPU��������֮ǰ�� uploadModel��
		newModel.uploadModel(m_allocator, this);
		return &newModel;
	}

	// 5. ����ʧ�������� map
	m_assetLibrary.erase(path);
	std::cerr << "Failed to import model: " << path << std::endl;
	return nullptr;
}

//void ErisEngine::drawWorld(VkCommandBuffer cmd, ErisWorld& world) {
//
//	FrameData& frame = getCurrentFrame();
//	
//	// 1. ��ȡͨ�õ� View �� Projection��һֻ֡�����һ�Σ�
//	float aspect = (float)m_swapchainExtent.width / (float)m_swapchainExtent.height;
//	glm::mat4 projection = m_camera.getProjectionMatrix(aspect);
//	glm::mat4 view = m_camera.getViewMatrix();
//
//	VkPipeline currentPipeline = isShadowPass ? m_shadowPipeline : m_pipeline;
//	VkPipelineLayout currentLayout = isShadowPass ? m_shadowPipelineLayout : m_pipelineLayout;
//
//	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, currentPipeline);
//	
//
//	VkViewport viewport{};
//	VkRect2D scissor{};
//	if (isShadowPass) {
//		viewport = { 0.f, 0.f, (float)m_shadowExtent.width, (float)m_shadowExtent.height, 0.f, 1.f };
//		scissor.extent = m_shadowExtent;
//	}
//	else {
//		viewport = { 0.f, 0.f, (float)m_swapchainExtent.width, (float)m_swapchainExtent.height, 0.f, 1.f };
//		scissor.extent = m_swapchainExtent;
//	}
//	scissor.offset = { 0, 0 };
//	vkCmdSetViewport(cmd, 0, 1, &viewport);
//	vkCmdSetScissor(cmd, 0, 1, &scissor);
//
//	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
//		currentLayout, 0, 1, &frame.sceneDescriptorSet, 0, nullptr);
//
//	// 2. �������������е�����
//	for (auto& objPtr : world.getObjects()) {
//		RenderObject& obj = *objPtr;
//		if (!obj.m_pModel) continue;
//
//		glm::mat4 modelMatrix = obj.getTransformMatrix();
//		MeshPushConstants constants;
//		if (isShadowPass) {
//			constants.render_matrix = modelMatrix;
//		}
//		else {
//			constants.render_matrix = projection * view * modelMatrix;
//		}
//		constants.model_matrix = modelMatrix;
//
//		// ���͵�ǰ����ľ���
//		vkCmdPushConstants(cmd, currentLayout, VK_SHADER_STAGE_VERTEX_BIT| VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(MeshPushConstants), &constants);
//
//
//		// --- ������ԭ������ mesh ��ѭ�� ---
//		for (auto& mesh : obj.m_pModel->meshes) {
//			if (mesh.vertexBuffer.buffer == VK_NULL_HANDLE) continue;
//			float rough = 0.5f, metal = 0.0f, emiss = 0.0f;
//			if (mesh.material) {
//				rough = mesh.material->roughness;
//				metal = mesh.material->metallic;
//				emiss = mesh.material->emission;
//			}
//			
//			MeshPushConstants constants;
//			constants.render_matrix = isShadowPass ? modelMatrix : projection * view * modelMatrix;
//			constants.model_matrix = modelMatrix;
//			// ���ؼ������� C++ �Ĳ������� Shader
//			constants.materialParams = glm::vec4(rough, metal, emiss, 0.0f);
//
//			// ���ؼ��������볣���� stageFlags ҲҪ���� FRAGMENT_BIT
//			vkCmdPushConstants(cmd, currentLayout,
//				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
//				0, sizeof(MeshPushConstants), &constants);
//
//
//			if (!isShadowPass) {
//				// ���������� (Set 0)
//				VkDescriptorSet set_to_bind = (mesh.material && mesh.material->textureSet != VK_NULL_HANDLE)
//					? mesh.material->textureSet : m_defaultTextureSet;
//
//				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 
//					currentLayout, 1, 1, &set_to_bind, 0, nullptr);
//			}
//
//
//			// �󶨶��������
//			VkBuffer vBuffers[] = { mesh.vertexBuffer.buffer };
//			VkDeviceSize offsets[] = { 0 };
//			vkCmdBindVertexBuffers(cmd, 0, 1, vBuffers, offsets);
//			vkCmdBindIndexBuffer(cmd, mesh.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
//			vkCmdDrawIndexed(cmd, static_cast<uint32_t>(mesh.indices.size()), 1, 0, 0, 0);
//		}
//	}
//
//	if (isShadowPass)return;
//
//	// ---------------------------------------------------------
//	// --- 3. ������պ� (��������֮��������Ȳ���) ---
//	// ---------------------------------------------------------
//	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_skyboxPipeline);
//
//	// ����պ���ͼ (Set 1)
//	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_skyboxPipelineLayout, 
//		0, 1, &frame.sceneDescriptorSet, 0, nullptr);
//
//	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_skyboxPipelineLayout,
//		1, 1, &m_skyboxDescriptorSet, 0, nullptr);
//
//	// ����պеĶ������� (m_skyboxMesh Ӧ������֮ǰ���ɵ�������)
//	VkBuffer skyBuffers[] = { m_skyboxMesh.vertexBuffer.buffer };
//	VkDeviceSize skyOffsets[] = { 0 };
//	vkCmdBindVertexBuffers(cmd, 0, 1, skyBuffers, skyOffsets);
//	vkCmdBindIndexBuffer(cmd, m_skyboxMesh.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
//
//	// ���� 36 �������������壩
//	vkCmdDrawIndexed(cmd, 36, 1, 0, 0, 0);
//
//	// ---------------------------------------------------------
//   // 2. ������������Ⱦ��������
//   // ---------------------------------------------------------
//	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gridPipeline);
//
//	// ֻ�� Set 0 (ȫ�� UBO)
//	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gridPipelineLayout, 0, 1, &frame.sceneDescriptorSet, 0, nullptr);
//
//	// ��Ϊ������ Shader �����ֱ�ӻ� 6 ���㼴��
//	vkCmdDraw(cmd, 6, 1, 0, 0);
//}

void ErisEngine::executeLumenCompositionPass(VkCommandBuffer cmd) {
	// --- 1. ���� 2 �������ֵ (�ӿ�ͼ + ���) ---
	std::array<VkClearValue, 2> clears{};
	clears[0].color.float32[0] = 0.0f; clears[0].color.float32[1] = 0.0f;
	clears[0].color.float32[2] = 0.0f; clears[0].color.float32[3] = 1.0f;
	clears[1].depthStencil.depth = 1.0f;

	VkRenderPassBeginInfo lightRpInfo{};
	lightRpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	lightRpInfo.renderPass = m_viewportPass;
	lightRpInfo.framebuffer = m_viewportFramebuffer;
	lightRpInfo.renderArea.extent.width = m_swapchainExtent.width;
	lightRpInfo.renderArea.extent.height = m_swapchainExtent.height;
	lightRpInfo.clearValueCount = 2;
	lightRpInfo.pClearValues = clears.data();

	// --- 2. ִ��ȫ���ϳ��߼� ---
	vkCmdBeginRenderPass(cmd, &lightRpInfo, VK_SUBPASS_CONTENTS_INLINE);

		drawLumenLighting(cmd); // ����ȫ�������ν��м���

		drawSkybox(cmd);        // �ںϳɺ�Ļ����ϵ�����պ�

		drawGrid(cmd);          // ������������

	vkCmdEndRenderPass(cmd);
}
void ErisEngine::executeGBufferPass(VkCommandBuffer cmd) {
	// --- 1. ��ʽ��ʼ�� 4 �����ֵ ---
	std::array<VkClearValue, 4> gClears{};
	// Position
	gClears[0].color.float32[0] = 0.0f; gClears[0].color.float32[1] = 0.0f;
	gClears[0].color.float32[2] = 0.0f; gClears[0].color.float32[3] = 1.0f;
	// Normal
	gClears[1].color.float32[0] = 0.0f; gClears[1].color.float32[1] = 0.0f;
	gClears[1].color.float32[2] = 0.0f; gClears[1].color.float32[3] = 1.0f;
	// Albedo
	gClears[2].color.float32[0] = 0.0f; gClears[2].color.float32[1] = 0.0f;
	gClears[2].color.float32[2] = 0.0f; gClears[2].color.float32[3] = 1.0f;
	// Depth
	gClears[3].depthStencil.depth = 1.0f; gClears[3].depthStencil.stencil = 0;

	// --- 2. ���ÿ�ʼ��Ϣ ---
	VkRenderPassBeginInfo gbufferRpInfo{};
	gbufferRpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	gbufferRpInfo.pNext = nullptr;
	gbufferRpInfo.renderPass = m_renderPass;
	gbufferRpInfo.framebuffer = m_gbufferFramebuffer;
	gbufferRpInfo.renderArea.offset.x = 0;
	gbufferRpInfo.renderArea.offset.y = 0;
	gbufferRpInfo.renderArea.extent.width = m_swapchainExtent.width;
	gbufferRpInfo.renderArea.extent.height = m_swapchainExtent.height;
	gbufferRpInfo.clearValueCount = static_cast<uint32_t>(gClears.size());
	gbufferRpInfo.pClearValues = gClears.data();

	// --- 3. ִ�л��� ---
	vkCmdBeginRenderPass(cmd, &gbufferRpInfo, VK_SUBPASS_CONTENTS_INLINE);
	// ʹ�� Lumen GBuffer ר�ù��߻�������
		drawMainGeometry(cmd, m_lumenGbufferPipeline, m_lumenGbufferPipelineLayout);
	vkCmdEndRenderPass(cmd);
}
void ErisEngine::executeEditorUIPass(VkCommandBuffer cmd, uint32_t imageIndex) {
	VkRenderPassBeginInfo uiRpInfo{};
	uiRpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	uiRpInfo.renderPass = m_uiRenderPass;
	uiRpInfo.framebuffer = m_frameBuffers[imageIndex];
	uiRpInfo.renderArea.extent.width = m_swapchainExtent.width;
	uiRpInfo.renderArea.extent.height = m_swapchainExtent.height;

	std::array<VkClearValue, 2> uiClears{};
	uiClears[0].color.float32[0] = 0.0f; uiClears[0].color.float32[1] = 0.0f;
	uiClears[0].color.float32[2] = 0.0f; uiClears[0].color.float32[3] = 1.0f;
	uiClears[1].depthStencil.depth = 1.0f;
	uiRpInfo.clearValueCount = 2;
	uiRpInfo.pClearValues = uiClears.data();

	vkCmdBeginRenderPass(cmd, &uiRpInfo, VK_SUBPASS_CONTENTS_INLINE);
		m_editor->draw(cmd); // ���Ʊ༭������	
	vkCmdEndRenderPass(cmd);
}
void ErisEngine::drawMainGeometry(VkCommandBuffer cmd, VkPipeline pipeline, VkPipelineLayout layout)
{
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(m_swapchainExtent.width);
	viewport.height = static_cast<float>(m_swapchainExtent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(cmd, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = m_swapchainExtent.width;
	scissor.extent.height = m_swapchainExtent.height;
	vkCmdSetScissor(cmd, 0, 1, &scissor);

	FrameData& frame = getCurrentFrame();
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

	// ��ȫ�ֻ������� (Set 0)
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &frame.sceneDescriptorSet, 0, nullptr);

	float aspect = static_cast<float>(m_swapchainExtent.width) / static_cast<float>(m_swapchainExtent.height);
	glm::mat4 view = m_camera.getViewMatrix();
	glm::mat4 proj = m_camera.getProjectionMatrix(aspect);

	for (auto& objPtr : m_activeWorld->getObjects()) {
		RenderObject& obj = *objPtr;
		if (obj.m_pModel == nullptr) continue;

		glm::mat4 modelMat = obj.getTransformMatrix();

		// ��ʽ���Ա��ֵ����
		MeshPushConstants constants;
		constants.render_matrix = proj * view * modelMat;
		constants.model_matrix = modelMat;

		// ��ʽ���ʲ�����ֵ
		float r = 0.5f; float m = 0.0f; float e = 0.0f;
		if (obj.m_pModel->meshes[0].material != nullptr) {
			Material* mat = obj.m_pModel->meshes[0].material;
			r = mat->roughness;
			m = mat->metallic;
			e = mat->emission;
		}
		constants.materialParams.x = r;
		constants.materialParams.y = m;
		constants.materialParams.z = e;
		constants.materialParams.w = 0.0f;

		vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(MeshPushConstants), &constants);

		for (auto& mesh : obj.m_pModel->meshes) {
			// �󶨲�����ͼ (Set 1)
			if (mesh.vertices.empty() || mesh.indices.empty()) continue;

			VkDescriptorSet texSet = (mesh.material && mesh.material->textureSet != VK_NULL_HANDLE)
				? mesh.material->textureSet : m_defaultTextureSet;
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 1, 1, &texSet, 0, nullptr);

			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(cmd, 0, 1, &mesh.vertexBuffer.buffer, &offset);
			vkCmdBindIndexBuffer(cmd, mesh.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(cmd, static_cast<uint32_t>(mesh.indices.size()), 1, 0, 0, 0);
		}
	}

}

void ErisEngine::drawSkybox(VkCommandBuffer cmd)
{
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(m_swapchainExtent.width);
	viewport.height = static_cast<float>(m_swapchainExtent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(cmd, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = m_swapchainExtent.width;
	scissor.extent.height = m_swapchainExtent.height;
	vkCmdSetScissor(cmd, 0, 1, &scissor);

	FrameData& frame = getCurrentFrame();
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_skyboxPipeline);

	// Set 0: Global, Set 1: Skybox Sampler
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_skyboxPipelineLayout, 0, 1, &frame.sceneDescriptorSet, 0, nullptr);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_skyboxPipelineLayout, 1, 1, &m_skyboxDescriptorSet, 0, nullptr);

	VkDeviceSize offset = 0;
	vkCmdBindVertexBuffers(cmd, 0, 1, &m_skyboxMesh.vertexBuffer.buffer, &offset);
	vkCmdBindIndexBuffer(cmd, m_skyboxMesh.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(cmd, 36, 1, 0, 0, 0);
}

void ErisEngine::drawShadow(VkCommandBuffer cmd)
{
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(m_shadowExtent.width);
	viewport.height = static_cast<float>(m_shadowExtent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(cmd, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = m_shadowExtent.width;
	scissor.extent.height = m_shadowExtent.height;
	vkCmdSetScissor(cmd, 0, 1, &scissor);

	FrameData& frame = getCurrentFrame();
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline);

	// ��Ӱֻ�� Set 0 ��� sunlightProj
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipelineLayout, 0, 1, &frame.sceneDescriptorSet, 0, nullptr);

	for (auto& objPtr : m_activeWorld->getObjects()) {
		RenderObject& obj = *objPtr;
		if (obj.m_pModel == nullptr) continue;

		MeshPushConstants constants;
		constants.model_matrix = obj.getTransformMatrix();
		constants.render_matrix = constants.model_matrix; // ��Ӱ Shader �����д��� sunlightProj

		vkCmdPushConstants(cmd, m_shadowPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(MeshPushConstants), &constants);

		for (auto& mesh : obj.m_pModel->meshes) {
			if (mesh.vertices.empty() || mesh.indices.empty()) continue;
			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(cmd, 0, 1, &mesh.vertexBuffer.buffer, &offset);
			vkCmdBindIndexBuffer(cmd, mesh.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(cmd, static_cast<uint32_t>(mesh.indices.size()), 1, 0, 0, 0);
		}
	}
}

void ErisEngine::drawGrid(VkCommandBuffer cmd)
{
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(m_swapchainExtent.width);
	viewport.height = static_cast<float>(m_swapchainExtent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(cmd, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = m_swapchainExtent.width;
	scissor.extent.height = m_swapchainExtent.height;
	vkCmdSetScissor(cmd, 0, 1, &scissor);

	FrameData& frame = getCurrentFrame();
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gridPipeline);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gridPipelineLayout, 0, 1, &frame.sceneDescriptorSet, 0, nullptr);
	vkCmdDraw(cmd, 6, 1, 0, 0);
}

void ErisEngine::drawLumenLighting(VkCommandBuffer cmd)
{
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(m_swapchainExtent.width);
	viewport.height = static_cast<float>(m_swapchainExtent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(cmd, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = m_swapchainExtent.width;
	scissor.extent.height = m_swapchainExtent.height;
	vkCmdSetScissor(cmd, 0, 1, &scissor);

	FrameData& frame = getCurrentFrame();
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_lumenLightingPipeline);

	// Set 0: Scene, Set 1: GBuffer
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_lumenLightingPipelineLayout, 0, 1, &frame.sceneDescriptorSet, 0, nullptr);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_lumenLightingPipelineLayout, 1, 1, &m_lumenDescriptorSet, 0, nullptr);

	// ֱ�ӻ� 3 ����������ȫ��������
	vkCmdDraw(cmd, 3, 1, 0, 0);
}


RenderObject* ErisEngine::pickObject(float mouseX, float mouseY)
{
	// mouseX, mouseY ����� ImGui �������� [0.0, 1.0] ����Ա�������

	// 1. ת��Ϊ NDC (Normalized Device Coordinates)
	// Vulkan �� NDC: x �� [-1, 1], y �� [-1, 1]
	float x_ndc = (mouseX * 2.0f) - 1.0f;
	float y_ndc = (mouseY * 2.0f) - 1.0f;

	// 2. ��ȡ�������
	float aspect = (float)m_swapchainExtent.width / (float)m_swapchainExtent.height;
	glm::mat4 projection = m_camera.getProjectionMatrix(aspect);
	glm::mat4 view = m_camera.getViewMatrix();

	// ����Ҫ��Vulkan ���е����������
	// ��Ⱦʱ����Ϊ��������Ļ���� projection[1][1] *= -1;
	// �ڼ�������ʱ�������ת���뱣������Ϊ����Ҫ����Ⱦ���Ļ�����ȫ���룡
	glm::mat4 invVP = glm::inverse(projection * view);

	// 3. ��������ռ��е�����
	// Vulkan ��ƽ�� z = 0.0, Զƽ�� z = 1.0 (���� Vulkan �� OpenGL ��������)
	glm::vec4 screenPosNear(x_ndc, y_ndc, 0.0f, 1.0f);
	glm::vec4 screenPosFar(x_ndc, y_ndc, 1.0f, 1.0f);

	// ͨ�������ת������ռ�
	glm::vec4 worldPosNear = invVP * screenPosNear;
	glm::vec4 worldPosFar = invVP * screenPosFar;

	// ͸�ӳ���
	worldPosNear /= worldPosNear.w;
	worldPosFar /= worldPosFar.w;

	// �õ�����ռ�����
	glm::vec3 rayOrigin = glm::vec3(worldPosNear);
	glm::vec3 rayDir = glm::normalize(glm::vec3(worldPosFar) - rayOrigin);

	// 4. �������
	RenderObject* closestObj = nullptr;
	float minDistance = FLT_MAX;

	for (auto& objPtr : m_activeWorld->getObjects()) {
		glm::vec3 bMin, bMax;
		// ��ȡ����������ռ䣨�Ѿ��˹� Model ���󣩵İ�Χ��
		objPtr->getWorldBounds(bMin, bMax);

		float t;
		// �ڡ�����ռ䡿ִ�������� AABB ����
		if (m_activeWorld->rayIntersectAABB(rayOrigin, rayDir, bMin, bMax, t)) {
			if (t < minDistance) {
				minDistance = t;
				closestObj = objPtr.get();
			}
		}
	}

	return closestObj;
}
