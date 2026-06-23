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
	initCommands();

	initRenderPass();
	initViewportPass();
	initViewportForwardPass();
	initUIRenderPass();
	createFramebuffers();

	initSyncStructures();
	initGbuffer();

	initDescriptors();
	initDescriptorPool();



	initGTAOPipeline();
	initForwardPipeline();
	initLumenGbufferPipeline();
	initLumenLightingPipeline();

	initSkyboxPipeline();
	initGridPipeline();
	initShadowResources();
	initShadowPipeline();
	createSceneBuffers();
	initSkyboxMesh();
	// Load BRDF LUT
	loadBRDFLUT(&m_brdfImageView);

	// 加载 irradiance map
	std::vector<std::string> irrFaces = {
		"assets/ibl/irradiance_posx.hdr",
		"assets/ibl/irradiance_negx.hdr",
		"assets/ibl/irradiance_posy.hdr",
		"assets/ibl/irradiance_negy.hdr",
		"assets/ibl/irradiance_posz.hdr",
		"assets/ibl/irradiance_negz.hdr",
	};
	m_irradianceMap = loadHDRCubemap(irrFaces);

	// 加载 prefiltered map（完整 9 级 mip 链）
	loadPrefilteredMap();

	m_activeWorld = new ErisWorld();
	m_activeWorld->getPhysics().createInfinitePlane();

	m_activeWorld->createDefaultSkybox();
	m_skyboxImage = loadCubemap(m_activeWorld->skyboxFaces);

	

	m_editor = new ErisEditor();
	m_editor->init(this);
	initViewportResources();
	// 如果没有加载出材质则用1x1白色代替
	initDefaultResources();
	// 初始化世界光
	initSceneData();
	
	initAOResources();

	updateSkyboxDescriptor();
	updateLumenDescriptorSet();
	updateForwardIBLDescriptorSet();

	Model* pSportCar = getOrLoadModel("assets/sportsCar/sportsCar.obj");
	Model* pGroundAsset = getOrLoadModel("assets/ground/churchfloor.obj");
	// set false
	if (!pGroundAsset) {
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
		car2->m_initialLocation = car2->m_location = glm::vec3(5.0f, 0.0f, -2.0f);
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

	uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;

	VkFormat imageFormat = VK_FORMAT_R8G8B8A8_SRGB; 
	VkExtent3D imageExtent;
	imageExtent.width = static_cast<uint32_t>(width);
	imageExtent.height = static_cast<uint32_t>(height);
	imageExtent.depth = 1;

	// 2. 创建 Staging Buffer
	AllocatedBuffer stagingbuffer = createBuffer(totalSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	void* data;
	vmaMapMemory(m_allocator, stagingbuffer.allocation, &data);
		for (int i = 0; i < 6; i++) {
			memcpy(static_cast<char*>(data) + (layerSize * i), pixels[i], layerSize);
			stbi_image_free(pixels[i]);
		}
	vmaUnmapMemory(m_allocator, stagingbuffer.allocation);

	// 3. 创建 GPU Image (关键：arrayLayers = 6, flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT)
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

		// C. 循环生成 Mipmaps
		int32_t mipWidth = width;
		int32_t mipHeight = height;

		for (uint32_t i = 1; i < mipLevels; i++) {
			// 1. 将上一层 (i-1) 从 DST 转为 SRC 供采样读取
			// layerCount=6, mipLevels=1, baseMipLevel=i-1
			transitionImageLayout(cmd, newImage.image, imageFormat,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				6, 1, i - 1);

			// 2. 执行 Blit (6 个面一起压缩)
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

			// 3. 上一层 (i-1) 已经用完，正式将其转为最终的 Shader 可读状态
			transitionImageLayout(cmd, newImage.image, imageFormat,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				6, 1, i - 1);

			if (mipWidth > 1) mipWidth /= 2;
			if (mipHeight > 1) mipHeight /= 2;
		}

		// D. 将最后一次生成的最小 Mip 层转为 Shader 可读
		transitionImageLayout(cmd, newImage.image, imageFormat,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			6, 1, mipLevels - 1);
		});


	// 5. 创建特殊的 ImageView (viewType = VK_IMAGE_VIEW_TYPE_CUBE)
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

AllocatedImage ErisEngine::loadHDRCubemap(const std::vector<std::string>& faces)
{
	if (faces.size() != 6) {
		throw std::runtime_error("Cubemap must have exactly 6 faces!");
	}

	int width, height, channels;
	std::vector<float*> pixels(6);

	// 加载 6 张 HDR 图
	for (int i = 0; i < 6; i++) {
		pixels[i] = stbi_loadf(faces[i].c_str(), &width, &height, &channels, STBI_rgb_alpha);
		if (!pixels[i])throw std::runtime_error("failed to load HDR cubemap face!");
	}

	VkDeviceSize layerSize = width * height * 4 * sizeof(float);
	VkDeviceSize totalSize = layerSize * 6;

	uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;

	VkFormat imageFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
	VkExtent3D imageExtent;
	imageExtent.width = static_cast<uint32_t>(width);
	imageExtent.height = static_cast<uint32_t>(height);
	imageExtent.depth = 1;

	// 创建 Staging Buffer
	AllocatedBuffer stagingbuffer = createBuffer(totalSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	
	void* data;
	vmaMapMemory(m_allocator, stagingbuffer.allocation, &data);
	for (int i = 0; i < 6; i++) {
		memcpy(static_cast<char*>(data) + (layerSize * i), pixels[i], layerSize);
		stbi_image_free(pixels[i]);
	}
	vmaUnmapMemory(m_allocator, stagingbuffer.allocation);

	// 创建 GPU Image
	AllocatedImage newImage;
	VkImageCreateInfo imgInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	imgInfo.imageType = VK_IMAGE_TYPE_2D;
	imgInfo.format = imageFormat;
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

		// 循环生成 Mipmaps
		int32_t mipWidth = width;
		int32_t mipHeight = height;

		for (uint32_t i = 1; i < mipLevels; i++) {
			transitionImageLayout(cmd, newImage.image, imageFormat,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				6, 1, i - 1);

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

			vkCmdBlitImage(cmd, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

			transitionImageLayout(cmd, newImage.image, imageFormat,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				6, 1, i - 1);

			if (mipWidth > 1) mipWidth /= 2;
			if (mipHeight > 1) mipHeight /= 2;
		}

		transitionImageLayout(cmd, newImage.image, imageFormat,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			6, 1, mipLevels - 1);
		});

	// 创建 ImageView
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

VkImage ErisEngine::loadBRDFLUT(VkImageView* outView)
{
	const int lutSize = 256;
	VkDeviceSize dataSize = lutSize * lutSize * 4; // R16G16 = 4 bytes/texel

	std::ifstream file("assets/ibl/brdf_lut.bin", std::ios::binary);
	if (!file) throw std::runtime_error("Failed to load BRDF LUT!");
	void* pixelData = new char[dataSize];
	file.read(static_cast<char*>(pixelData), dataSize);
	file.close();

	AllocatedBuffer stagingBuffer = createBuffer(dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	void* mapped;
	vmaMapMemory(m_allocator, stagingBuffer.allocation, &mapped);
	memcpy(mapped, pixelData, dataSize);
	vmaUnmapMemory(m_allocator, stagingBuffer.allocation);
	delete[] pixelData;

	VkImage image;
	VmaAllocation allocation;
	VkImageCreateInfo imgInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	imgInfo.imageType = VK_IMAGE_TYPE_2D;
	imgInfo.format = VK_FORMAT_R16G16_SFLOAT;
	imgInfo.extent = { (uint32_t)lutSize, (uint32_t)lutSize, 1 };
	imgInfo.mipLevels = 1;
	imgInfo.arrayLayers = 1;
	imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

	VmaAllocationCreateInfo allocInfo{ VMA_MEMORY_USAGE_GPU_ONLY };
	vmaCreateImage(m_allocator, &imgInfo, &allocInfo, &image, &allocation, nullptr);

	immediateSubmit([&](VkCommandBuffer cmd) {
		VkBufferImageCopy region{};
		region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		region.imageExtent = { (uint32_t)lutSize, (uint32_t)lutSize, 1 };

		transitionImageLayout(cmd, image, VK_FORMAT_R16G16_SFLOAT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, 1, 0);
		vkCmdCopyBufferToImage(cmd, stagingBuffer.buffer, image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
		transitionImageLayout(cmd, image, VK_FORMAT_R16G16_SFLOAT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			1, 1, 0);
		});

	VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = VK_FORMAT_R16G16_SFLOAT;
	viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	vkCreateImageView(m_device, &viewInfo, nullptr, outView);

	m_mainDeletionQueue.push_function([=]() {
		vkDestroyImageView(m_device, *outView, nullptr);
		vmaDestroyImage(m_allocator, image, allocation);
		});

	return image;
}

// 该函数加载 prefiltered 贴图的 9 级 mip x 6 面 = 54 个 .hdr 文件，不使用 blit 生成 mip，完整保留 GGX 预过滤数据。

void ErisEngine::loadPrefilteredMap()
{
	const int numMips = 9;
	const int baseSize = 256;
	const char* faceSuffixes[6] = { "posx","negx","posy","negy","posz","negz" };
	std::string basePath = "assets/ibl";

	// 1. 计算 staging buffer 总大小
	VkDeviceSize totalSize = 0;
	int mipSizes[9];
	for (int mip = 0; mip < numMips; mip++) {
		int size = std::max(baseSize >> mip, 1);
		mipSizes[mip] = size;
		totalSize += size * size * 4 * sizeof(float) * 6;
	}

	// 2. 创建 staging buffer，加载所有 54 个文件
	AllocatedBuffer staging = createBuffer(totalSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	void* mapped;
	vmaMapMemory(m_allocator, staging.allocation, &mapped);
	VkDeviceSize offset = 0;

	for (int mip = 0; mip < numMips; mip++) {
		int sz = mipSizes[mip];
		VkDeviceSize faceSize = sz * sz * 4 * sizeof(float);
		for (int face = 0; face < 6; face++) {
			char filename[256];
			sprintf_s(filename, "%s/prefiltered_%s_%d_%dx%d.hdr",
				basePath.c_str(), faceSuffixes[face], mip, sz, sz);
			int w, h, c;
			float* pixels = stbi_loadf(filename, &w, &h, &c, STBI_rgb_alpha);
			if (!pixels) {
				throw std::runtime_error(std::string("Failed: ") + filename);
			}
			memcpy(static_cast<char*>(mapped) + offset, pixels, faceSize);
			stbi_image_free(pixels);
			offset += faceSize;
		}
	}
	vmaUnmapMemory(m_allocator, staging.allocation);

	// 3. 创建 GPU cubemap，mipLevels = numMips
	AllocatedImage newImage;
	VkImageCreateInfo imgInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	imgInfo.imageType = VK_IMAGE_TYPE_2D;
	imgInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	imgInfo.extent = { (uint32_t)baseSize, (uint32_t)baseSize, 1 };
	imgInfo.mipLevels = (uint32_t)numMips;
	imgInfo.arrayLayers = 6;
	imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imgInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

	VmaAllocationCreateInfo allocInfo{ VMA_MEMORY_USAGE_GPU_ONLY };
	vmaCreateImage(m_allocator, &imgInfo, &allocInfo, &newImage.image, &newImage.allocation, nullptr);

	// 4. 逐 mip 逐面上传（不走 blit）
	offset = 0;
	immediateSubmit([&](VkCommandBuffer cmd) {
		for (uint32_t mip = 0; mip < (uint32_t)numMips; mip++) {
			int sz = std::max(baseSize >> mip, 1);
			VkDeviceSize faceSize = sz * sz * 4 * sizeof(float);
			VkExtent3D mipExtent = { (uint32_t)sz, (uint32_t)sz, 1 };

			transitionImageLayout(cmd, newImage.image, VK_FORMAT_R32G32B32A32_SFLOAT,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				6, 1, mip);

			std::vector<VkBufferImageCopy> regions(6);
			for (uint32_t f = 0; f < 6; f++) {
				regions[f].bufferOffset = offset + faceSize * f;
				regions[f].imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, mip, f, 1 };
				regions[f].imageExtent = mipExtent;
			}
			vkCmdCopyBufferToImage(cmd, staging.buffer, newImage.image,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 6, regions.data());

			transitionImageLayout(cmd, newImage.image, VK_FORMAT_R32G32B32A32_SFLOAT,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				6, 1, mip);

			offset += faceSize * 6;
		}
		});

	// 5. ImageView
	VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	viewInfo.image = newImage.image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
	viewInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, (uint32_t)numMips, 0, 6 };
	vkCreateImageView(m_device, &viewInfo, nullptr, &newImage.imageView);

	m_mainDeletionQueue.push_function([=]() {
		vkDestroyImageView(m_device, newImage.imageView, nullptr);
		vmaDestroyImage(m_allocator, newImage.image, newImage.allocation);
		});
	// vmaDestroyBuffer(m_allocator, staging.buffer, staging.allocation);

	m_prefilteredMap = newImage;
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
	initShadowResources();
	createSceneBuffers();
	initViewportResources();
	initAOResources();
	initGbuffer();
	updateLumenDescriptorSet();
	updateForwardIBLDescriptorSet();
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
		shadowWrite.dstBinding = 1;											// 对应 Shader 里的 layout(set=0, binding=1)
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

void ErisEngine::initAOResources()
{
	// 1. 创建 AO 存储图像
	VkImageCreateInfo imgInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	imgInfo.imageType = VK_IMAGE_TYPE_2D;
	imgInfo.format = VK_FORMAT_R8_UNORM;
	imgInfo.extent = { m_swapchainExtent.width,m_swapchainExtent.height,1 };
	imgInfo.mipLevels = 1;
	imgInfo.arrayLayers = 1;
	imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imgInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

	VmaAllocationCreateInfo allocInfo{ VMA_MEMORY_USAGE_GPU_ONLY };
	vmaCreateImage(m_allocator, &imgInfo, &allocInfo, &m_aoImage.image, &m_aoImage.allocation, nullptr);
	m_aoImage.imageView = createImageView(m_aoImage.image, VK_FORMAT_R8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1);
	
    // 2. 分配 GTAO 描述符集
    VkDescriptorSetAllocateInfo dsAlloc{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    dsAlloc.descriptorPool = m_descriptorPool;
    dsAlloc.descriptorSetCount = 1;
    dsAlloc.pSetLayouts = &m_gtaoDescriptorSetLayout;
    vkAllocateDescriptorSets(m_device, &dsAlloc, &m_gtaoDescriptorSet);

	// Binding 0: G-buffer normal
	VkDescriptorImageInfo normalInfo{};
	normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	normalInfo.imageView = m_gbuffer.normal.imageView;
	normalInfo.sampler = m_sampler;

	// Binding 1: Depth
	VkDescriptorImageInfo depthInfo{};
	depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	depthInfo.imageView = m_depthImage.imageView;
	depthInfo.sampler = m_sampler;

	// Binding 2: AO output (storage image)
	VkDescriptorImageInfo aoInfo{};
	aoInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	aoInfo.imageView = m_aoImage.imageView;

	// 3. 写入三个描述符
	std::array<VkWriteDescriptorSet, 3> writes{};

	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = m_gtaoDescriptorSet;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[0].pImageInfo = &normalInfo;

	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = m_gtaoDescriptorSet;
	writes[1].dstBinding = 1;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[1].pImageInfo = &depthInfo;


	writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[2].dstSet = m_gtaoDescriptorSet;
	writes[2].dstBinding = 2;
	writes[2].descriptorCount = 1;
	writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[2].pImageInfo = &aoInfo;

	vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
	
	//std::cout << m_depthImage.image << std::endl;

	immediateSubmit([&](VkCommandBuffer cmd) {
		transitionImageLayout(cmd, m_aoImage.image, VK_FORMAT_R8_UNORM,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1, 0);
		});

	m_mainDeletionQueue.push_function([=]() {
		vkDestroyImageView(m_device, m_aoImage.imageView, nullptr);
		vmaDestroyImage(m_allocator, m_aoImage.image, m_aoImage.allocation);
		});
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

void ErisEngine::initForwardPipeline()
{
	VkShaderModule vertModule, fragModule;
	if (!loadShaderModule("shaders/vert.spv", &vertModule) || !loadShaderModule("shaders/frag.spv", &fragModule)) {
		throw std::runtime_error("failed to load shaders!");
	}

	std::array<VkDescriptorSetLayout, 3> layouts = { m_globalSetLayout,m_singleTextureLayout,m_iblDescriptorSetLayout };

	// 管线布局
	VkPushConstantRange pushConstantRange;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(MeshPushConstants);
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

	VkPipelineLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.setLayoutCount = static_cast<uint32_t> (layouts.size());
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
	builder.m_rasterizer.cullMode = VK_CULL_MODE_NONE;
	builder.m_rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;	// vulkan翻转Y轴了
	builder.m_rasterizer.depthBiasEnable = VK_FALSE;
	builder.m_rasterizer.depthBiasConstantFactor = 0.0f;
	builder.m_rasterizer.depthBiasClamp = 0.0f;
	builder.m_rasterizer.depthBiasSlopeFactor = 0.0f;
	builder.m_rasterizer.lineWidth = 1.0f;

	// 6. 混合模式（不透明）
	VkPipelineColorBlendAttachmentState blend{};
	blend.blendEnable = VK_FALSE;
	blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	builder.m_colorBlendAttachments.clear();
	builder.m_colorBlendAttachments.push_back(blend);

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
	skyBuilder.m_depthStencil.depthWriteEnable = VK_FALSE; // 关键：天空盒不写入深度，不遮挡后续UI
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
	builder.m_depthStencil.depthTestEnable = VK_TRUE;  // 必须开启，用于检测是否被车遮挡
	builder.m_depthStencil.depthWriteEnable = VK_FALSE; // 必须关闭，因为网格是透明的
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
	m_shadowExtent = { 2048, 2048 }; // 阴影图尺寸

	// 1. 创建阴影深度图像 (使用 VMA)
	VkImageCreateInfo imgInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	imgInfo.imageType = VK_IMAGE_TYPE_2D;
	imgInfo.format = m_depthFormat; // 建议 D32_SFLOAT
	imgInfo.extent = { m_shadowExtent.width, m_shadowExtent.height, 1 };
	imgInfo.mipLevels = 1;
	imgInfo.arrayLayers = 1;
	imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	// 关键：既是深度附件，又要作为贴图被采样
	imgInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;


	VmaAllocationCreateInfo vmaAllocInfo{ VMA_MEMORY_USAGE_GPU_ONLY };
	vmaCreateImage(m_allocator, &imgInfo, &vmaAllocInfo,
		&m_shadowImage.image, &m_shadowImage.allocation, nullptr);

	// 2. 创建图像视图
	m_shadowImage.imageView = createImageView(m_shadowImage.image, m_depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);

	// 3. 创建阴影 RenderPass (仅处理深度)
	VkAttachmentDescription depthAttachment{};
	depthAttachment.format = m_depthFormat;
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;    // 每一帧开始清空
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // 存下来供主 Pass 读取
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // 画完直接进入读取状态

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

	// 4. 创建阴影 Framebuffer
	VkFramebufferCreateInfo fbInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
	fbInfo.renderPass = m_shadowRenderPass;
	fbInfo.attachmentCount = 1;
	fbInfo.pAttachments = &m_shadowImage.imageView;
	fbInfo.width = m_shadowExtent.width;
	fbInfo.height = m_shadowExtent.height;
	fbInfo.layers = 1;

	vkCreateFramebuffer(m_device, &fbInfo, nullptr, &m_shadowFrameBuffer);

	// 5. 注册销毁
	// cleanup moved to cleanSwapchain()
}

void ErisEngine::initShadowPipeline()
{
	VkShaderModule shadowVert;
	if (!loadShaderModule("shaders/shadow_vert.spv", &shadowVert)) {
		throw std::runtime_error("failed to load shadow vert shader!");
	}
	// 为什么是m_globalSetLayout
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

// Build compute pipeline
void ErisEngine::initGTAOPipeline()
{
	// 1. 加载 compute shader
	VkShaderModule compModule;
	if (!loadShaderModule("shaders/gtao.spv", &compModule)) {
		throw std::runtime_error("failed to load gtao compute shader!");
	}


	// 2. GTAO 描述符集布局 (set=1): normal + depth + AO
	std::array<VkDescriptorSetLayoutBinding, 3> binds{};

	binds[0].binding = 0;
	binds[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[0].descriptorCount = 1;
	binds[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	binds[1].binding = 1;
	binds[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[1].descriptorCount = 1;
	binds[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	binds[2].binding = 2;
	binds[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	binds[2].descriptorCount = 1;
	binds[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkDescriptorSetLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	layoutInfo.bindingCount = static_cast<uint32_t>(binds.size());
	layoutInfo.pBindings = binds.data();
	vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_gtaoDescriptorSetLayout);
	std::cout << "gtaoDescLayout=" << m_gtaoDescriptorSetLayout << std::endl;

    // 3. Pipeline Layout: set=0 (场景 UBO) + set=1 (GTAO) + Push Constants
    VkDescriptorSetLayout setLayouts[] = { m_globalSetLayout, m_gtaoDescriptorSetLayout };

    VkPushConstantRange pcRange{};
    pcRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pcRange.offset = 0;
    pcRange.size = sizeof(glm::mat4) + sizeof(glm::vec4) + sizeof(glm::vec2);
    // mat4 invProj (64) + vec4 params (16) + vec2 screenSize (8) = 88 bytes

    VkPipelineLayoutCreateInfo plInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    plInfo.setLayoutCount = 2;
    plInfo.pSetLayouts = setLayouts;
    plInfo.pushConstantRangeCount = 1;
    plInfo.pPushConstantRanges = &pcRange;
    vkCreatePipelineLayout(m_device, &plInfo, nullptr, &m_gtaoPipelineLayout);

    // 4. Compute Pipeline
    ComputePipelineBuilder builder;
    builder.m_pipelineLayout = m_gtaoPipelineLayout;
    m_gtaoPipeline = builder.buildComputePipeline(m_device, compModule);
    vkDestroyShaderModule(m_device, compModule, nullptr);

    // 5. 删除队列
    m_mainDeletionQueue.push_function([=]() {
        vkDestroyPipeline(m_device, m_gtaoPipeline, nullptr);
        vkDestroyPipelineLayout(m_device, m_gtaoPipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(m_device, m_gtaoDescriptorSetLayout, nullptr);
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

	// 1. 定义 4 个附件 (3 个颜色，1 个深度)
	std::array<VkAttachmentDescription, 4> attachments = {
		createAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL), // Position
		createAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL), // Normal
		createAttachment(VK_FORMAT_R8G8B8A8_UNORM,      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL), // Albedo
		createAttachment(m_depthFormat,                 VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) // Depth
	};

	// 2. 定义附件引用
	std::array<VkAttachmentReference, 3> colorRefs = {
		VkAttachmentReference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
		VkAttachmentReference{1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
		VkAttachmentReference{2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}
	};
	VkAttachmentReference depthRef = { 3, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };


	// 3. 子通道配置 (绑定多个颜色目标)
	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = static_cast<uint32_t>(colorRefs.size());
	subpass.pColorAttachments = colorRefs.data();
	subpass.pDepthStencilAttachment = &depthRef;
	
	// 4. 依赖关系 (保证上一帧写完后再读)
	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	
	// 5. 创建 RenderPass
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

void ErisEngine::initViewportPass()
{
	// 1. 颜色附件 (最终合成后的画面)
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = m_swapchainImageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// 2. 深度附件
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

void ErisEngine::initViewportForwardPass()
{
	// 1. 颜色附件 (最终合成后的画面)
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = m_swapchainImageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// 2. 深度附件
	VkAttachmentDescription depthAttachment{};
	depthAttachment.format = m_depthFormat;
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
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

	vkCreateRenderPass(m_device, &createInfo, nullptr, &m_viewportForwardPass);

	m_mainDeletionQueue.push_function([=]() {
		vkDestroyRenderPass(m_device, m_viewportForwardPass, nullptr);
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
	depthImageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

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
	appInfo.apiVersion = VK_API_VERSION_1_4;


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
	// 1. 创建全局以及阴影布局 (Set 0) - 用于 GPUSceneData
	// ---------------------------------------------------------
	VkDescriptorSetLayoutBinding sceneBinds[2];

    // Binding 0: 全局 UBO 数据
	sceneBinds[0].binding = 0;
	sceneBinds[0].descriptorCount = 1;
	sceneBinds[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	sceneBinds[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;

	// Binding 1: 阴影深度贴图 (Sampler)
	sceneBinds[1].binding = 1;
	sceneBinds[1].descriptorCount = 1;
	sceneBinds[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	sceneBinds[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
	sceneBinds[1].pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo globalLayoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	globalLayoutInfo.bindingCount = 2;
	globalLayoutInfo.pBindings = sceneBinds;
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


	// ---------------------------------------------------------
	// 2.3 IBL 描述符布局 (Set 2) - skybox + BRDF LUT
	// ---------------------------------------------------------
	std::vector<VkDescriptorSetLayoutBinding> iblBinds;

	VkDescriptorSetLayoutBinding iblSkyBinding{};
	iblSkyBinding.binding = 0;
	iblSkyBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	iblSkyBinding.descriptorCount = 1;
	iblSkyBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	iblSkyBinding.pImmutableSamplers = nullptr;
	iblBinds.push_back(iblSkyBinding);

	VkDescriptorSetLayoutBinding iblBRDFBinding{};
	iblBRDFBinding.binding = 1;
	iblBRDFBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	iblBRDFBinding.descriptorCount = 1;
	iblBRDFBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	iblBRDFBinding.pImmutableSamplers = nullptr;
	iblBinds.push_back(iblBRDFBinding);

	VkDescriptorSetLayoutBinding fIrrBind{};
	fIrrBind.binding = 2;
	fIrrBind.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	fIrrBind.descriptorCount = 1;
	fIrrBind.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	iblBinds.push_back(fIrrBind);

	VkDescriptorSetLayoutBinding fPreBind{};
	fPreBind.binding = 3;
	fPreBind.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	fPreBind.descriptorCount = 1;
	fPreBind.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	iblBinds.push_back(fPreBind);


	VkDescriptorSetLayoutCreateInfo iblLayoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	iblLayoutInfo.bindingCount = static_cast<uint32_t>(iblBinds.size());
	iblLayoutInfo.pBindings = iblBinds.data();
	vkCreateDescriptorSetLayout(m_device, &iblLayoutInfo, nullptr, &m_iblDescriptorSetLayout);

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


	VkDescriptorSetLayoutBinding brdfBind{};
	brdfBind.binding = 4;
	brdfBind.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	brdfBind.descriptorCount = 1;
	brdfBind.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	brdfBind.pImmutableSamplers = nullptr;
	lumenBinds.push_back(brdfBind);

	VkDescriptorSetLayoutBinding irrBind{};
	irrBind.binding = 5;
	irrBind.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	irrBind.descriptorCount = 1;
	irrBind.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	lumenBinds.push_back(irrBind);

	VkDescriptorSetLayoutBinding preBind{};
	preBind.binding = 6;
	preBind.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	preBind.descriptorCount = 1;
	preBind.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	lumenBinds.push_back(preBind);

	VkDescriptorSetLayoutBinding aoLumenBind{};
	aoLumenBind.binding = 7;
	aoLumenBind.descriptorType= VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	aoLumenBind.descriptorCount = 1;
	aoLumenBind.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	lumenBinds.push_back(aoLumenBind);

	VkDescriptorSetLayoutCreateInfo lumenLayoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	lumenLayoutInfo.bindingCount = static_cast<uint32_t>(lumenBinds.size());
	lumenLayoutInfo.pBindings = lumenBinds.data();
	vkCreateDescriptorSetLayout(m_device, &lumenLayoutInfo, nullptr, &m_lumenDescriptorLayout);


	// 注册销毁 (注意顺序：先删 Pool 再删 Layout)
	m_mainDeletionQueue.push_function([=]() {
		vkDestroySampler(m_device, m_sampler, nullptr);
		vkDestroySampler(m_device, m_skyboxSampler, nullptr);
		vkDestroyDescriptorSetLayout(m_device, m_globalSetLayout, nullptr);
		vkDestroyDescriptorSetLayout(m_device, m_singleTextureLayout, nullptr);
		vkDestroyDescriptorSetLayout(m_device, m_skyboxDescriptorSetLayout, nullptr);
		vkDestroyDescriptorSetLayout(m_device, m_lumenDescriptorLayout, nullptr);
		vkDestroyDescriptorSetLayout(m_device, m_iblDescriptorSetLayout, nullptr);
		});
}

void ErisEngine::initDescriptorPool()
{
	std::vector<VkDescriptorPoolSize>sizes = {
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,100},				// 支持 100 个全局数据块
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,500},		// 支持 500 张材质贴图
		{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,10}
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

void ErisEngine::initGbuffer()
{
	// 创建三张和屏幕一样大的贴图

	VkExtent3D extent = { m_swapchainExtent.width, m_swapchainExtent.height, 1 };

	// 1. 位置缓冲 (需要高精度，用 16 位浮点数)
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

	// 2. 法线缓冲 (同样需要高精度存法线方向，外加金属度)
	VkImageCreateInfo normInfo = posInfo;
	vmaCreateImage(m_allocator, &normInfo, &allocInfo, &m_gbuffer.normal.image, &m_gbuffer.normal.allocation, nullptr);
	m_gbuffer.normal.imageView = createImageView(m_gbuffer.normal.image, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 1);

	// 3. 颜色/粗糙度缓冲 (8位就够了)
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

	immediateSubmit([&](VkCommandBuffer cmd) {
		transitionImageLayout(cmd, m_gbuffer.position.image, VK_FORMAT_R16G16B16A16_SFLOAT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1, 0);
		transitionImageLayout(cmd, m_gbuffer.normal.image, VK_FORMAT_R16G16B16A16_SFLOAT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1, 0);
		transitionImageLayout(cmd, m_gbuffer.albedo.image, VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1, 0);
		});
	// 注册销毁
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
	skyInfo.sampler = m_skyboxSampler; // 使用你初始化好的天空盒采样器
	skyInfo.imageView = m_skyboxImage.imageView;
	skyInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkDescriptorImageInfo brdfInfo{};
	brdfInfo.sampler = m_sampler;
	brdfInfo.imageView = m_brdfImageView;
	brdfInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkDescriptorImageInfo irrInfo{};
	irrInfo.sampler = m_skyboxSampler;
	irrInfo.imageView = m_irradianceMap.imageView;
	irrInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkDescriptorImageInfo preInfo{};
	preInfo.sampler = m_skyboxSampler;
	preInfo.imageView = m_prefilteredMap.imageView;
	preInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkDescriptorImageInfo aoLumenInfo{};
	aoLumenInfo.sampler = m_sampler;
	aoLumenInfo.imageView = m_aoImage.imageView;
	aoLumenInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	std::array<VkWriteDescriptorSet, 8> writes{};

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

	writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[4].dstSet = m_lumenDescriptorSet;
	writes[4].dstBinding = 4;
	writes[4].descriptorCount = 1;
	writes[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[4].pImageInfo = &brdfInfo;

	writes[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[5].dstSet = m_lumenDescriptorSet;
	writes[5].dstBinding = 5;
	writes[5].descriptorCount = 1;
	writes[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[5].pImageInfo = &irrInfo;

	writes[6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[6].dstSet = m_lumenDescriptorSet;
	writes[6].dstBinding = 6;
	writes[6].descriptorCount = 1;
	writes[6].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[6].pImageInfo = &preInfo;

	writes[7].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[7].dstSet = m_lumenDescriptorSet;
	writes[7].dstBinding = 7;
	writes[7].descriptorCount = 1;
	writes[7].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[7].pImageInfo = &aoLumenInfo;

	vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void ErisEngine::updateForwardIBLDescriptorSet()
{
	// 分配 IBL 描述符集 (Set 2 for Forward)
	VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	allocInfo.descriptorPool = m_descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &m_iblDescriptorSetLayout;
	vkAllocateDescriptorSets(m_device, &allocInfo, &m_iblDescriptorSet);

	// Skybox cubemap
	VkDescriptorImageInfo iblSkyInfo{};
	iblSkyInfo.sampler = m_skyboxSampler;
	iblSkyInfo.imageView = m_skyboxImage.imageView;
	iblSkyInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// BRDF LUT
	VkDescriptorImageInfo iblBRDFInfo{};
	iblBRDFInfo.sampler = m_sampler;
	iblBRDFInfo.imageView = m_brdfImageView;
	iblBRDFInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkDescriptorImageInfo irrInfo{};
	irrInfo.sampler = m_skyboxSampler;
	irrInfo.imageView = m_irradianceMap.imageView;
	irrInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkDescriptorImageInfo preInfo{};
	preInfo.sampler = m_skyboxSampler;
	preInfo.imageView = m_prefilteredMap.imageView;
	preInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;


	std::array<VkWriteDescriptorSet, 4> iblWrites{};
	iblWrites[0] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	iblWrites[0].dstSet = m_iblDescriptorSet;
	iblWrites[0].dstBinding = 0;
	iblWrites[0].descriptorCount = 1;
	iblWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	iblWrites[0].pImageInfo = &iblSkyInfo;

	iblWrites[1] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	iblWrites[1].dstSet = m_iblDescriptorSet;
	iblWrites[1].dstBinding = 1;
	iblWrites[1].descriptorCount = 1;
	iblWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	iblWrites[1].pImageInfo = &iblBRDFInfo;

	iblWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	iblWrites[2].dstSet = m_iblDescriptorSet;
	iblWrites[2].dstBinding = 2;
	iblWrites[2].descriptorCount = 1;
	iblWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	iblWrites[2].pImageInfo = &irrInfo;

	iblWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	iblWrites[3].dstSet = m_iblDescriptorSet;
	iblWrites[3].dstBinding = 3;
	iblWrites[3].descriptorCount = 1;
	iblWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	iblWrites[3].pImageInfo = &preInfo;

	vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(iblWrites.size()), iblWrites.data(), 0, nullptr);
}

void ErisEngine::initLumenLightingPipeline()
{
	// 加载我们刚写的 lumen_lighting.vert 和 lumen_main.frag
	VkShaderModule vertMod, fragMod;
	if (!loadShaderModule("shaders/Lumen/main_vert.spv", &vertMod) || !loadShaderModule("shaders/Lumen/main_frag.spv", &fragMod)) {
		throw std::runtime_error("failed to load lumen light shader module!");
	}

	// 布局设计：Set 0 是全局数据(SceneData), Set 1 是 GBuffer
	std::array<VkDescriptorSetLayout, 2> layouts = { m_globalSetLayout, m_lumenDescriptorLayout };

	VkPipelineLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	layoutInfo.setLayoutCount = static_cast<uint32_t>(layouts.size());
	layoutInfo.pSetLayouts = layouts.data();
	vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_lumenLightingPipelineLayout);

	PipelineBuilder builder;
	builder.m_shaderStages.push_back({ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertMod, "main" });
	builder.m_shaderStages.push_back({ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragMod, "main" });

	// 【关键】：这里不需要任何顶点输入
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


	// 颜色混合 (输出到最终视口图)
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

	// 2. 构建管线
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

	// 配置 3 个混合附件 (MRT 必须匹配)
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

	// 绑定到那个支持 4 附件（3色1深）的 RenderPass
	m_lumenGbufferPipeline = builder.buildPipeline(m_device, m_renderPass);

	vkDestroyShaderModule(m_device, vertMod, nullptr);
	vkDestroyShaderModule(m_device, fragMod, nullptr);

	m_mainDeletionQueue.push_function([=]() {
		vkDestroyPipelineLayout(m_device, m_lumenGbufferPipelineLayout, nullptr);
		vkDestroyPipeline(m_device, m_lumenGbufferPipeline, nullptr);
		});

}
void ErisEngine::executeShadowPass(VkCommandBuffer cmd) {
	// --- 1. 显式配置 RenderPass 开始信息 ---
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

	// --- 2. 执行绘制 ---
	vkCmdBeginRenderPass(cmd, &shadowRpInfo, VK_SUBPASS_CONTENTS_INLINE);
		drawShadow(cmd); 
	vkCmdEndRenderPass(cmd);

	// --- 3. 显式同步：阴影写完后转为 Shader 可读 ---
	transitionImageLayout(cmd, m_shadowImage.image, m_depthFormat,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1, 0);
}

void ErisEngine::executeForwardPass(VkCommandBuffer cmd) {
	// --- 1. 同步：将视口图从“读取”转为“写入”状态 ---
	transitionImageLayout(cmd, m_viewportImage.image, m_swapchainImageFormat,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1,1,0);

	// --- 2. 配置清除值 (1个背景色 + 1个深度) ---
	std::array<VkClearValue, 2> clears{};
	// 背景色：暗灰色
	clears[0].color.float32[0] = 0.03f;
	clears[0].color.float32[1] = 0.03f;
	clears[0].color.float32[2] = 0.03f;
	clears[0].color.float32[3] = 1.0f;
	// 深度
	clears[1].depthStencil.depth = 1.0f;
	clears[1].depthStencil.stencil = 0;

	// --- 3. 配置 RenderPass 开始信息 ---
	VkRenderPassBeginInfo sceneRpInfo{};
	sceneRpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	sceneRpInfo.pNext = nullptr;
	sceneRpInfo.renderPass = m_viewportForwardPass; // 使用单附件合成 Pass
	sceneRpInfo.framebuffer = m_viewportFramebuffer;
	sceneRpInfo.renderArea.offset.x = 0;
	sceneRpInfo.renderArea.offset.y = 0;
	sceneRpInfo.renderArea.extent.width = m_swapchainExtent.width;
	sceneRpInfo.renderArea.extent.height = m_swapchainExtent.height;
	sceneRpInfo.clearValueCount = static_cast<uint32_t>(clears.size());
	sceneRpInfo.pClearValues = clears.data();

	// --- 4. 执行绘制流程 ---
	vkCmdBeginRenderPass(cmd, &sceneRpInfo, VK_SUBPASS_CONTENTS_INLINE);

		// A. 画模型 (使用 Forward 专用管线)

		// 绑定 IBL 描述符集 (Set 2)
		
		drawMainGeometry(cmd, m_pipeline, m_pipelineLayout);

		// B. 画天空盒
		drawSkybox(cmd);

		// C. 画网格
		drawGrid(cmd);

	vkCmdEndRenderPass(cmd);

	// --- 5. 同步：渲染结束后自动转回 SHADER_READ_ONLY 供 ImGui 使用 ---
	// (因为 m_viewportPass 的 finalLayout 已经配置好了，此处无需手动转换)
	transitionImageLayout(cmd, m_depthImage.image, m_depthFormat,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, 1,1,0);
}

void ErisEngine::executeGTAOPass(VkCommandBuffer cmd)
{
	// 1. 过渡 depth → DEPTH_STENCIL_READ_ONLY（G-buffer pass 后 depth 处于 ATTACHMENT_OPTIMAL）
	transitionImageLayout(cmd, m_depthImage.image, m_depthFormat,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, 1, 1, 0);

	// 2. 过渡 AO 图 → GENERAL（storage image 需要）
	transitionImageLayout(cmd, m_aoImage.image, VK_FORMAT_R8_UNORM,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 1, 1, 0);

	// 3. 准备 push constants
	glm::mat4 invProj = glm::inverse(m_sceneData.proj);

	struct GTAOPushConstants {
		glm::mat4 invProj;
		glm::vec4 params;   // x: stepRadius, y: contrast, z: bias, w: pad
		glm::vec2 screenSize;
	} pc;
	pc.invProj = invProj;
	pc.params = glm::vec4(1.5f, 1.5f, 0.001f, 0.0f);
	pc.screenSize = glm::vec2(m_swapchainExtent.width, m_swapchainExtent.height);

	// 4. Dispatch GTAO compute
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_gtaoPipeline);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
		m_gtaoPipelineLayout, 0, 1, &getCurrentFrame().sceneDescriptorSet, 0, nullptr);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
		m_gtaoPipelineLayout, 1, 1, &m_gtaoDescriptorSet, 0, nullptr);
	vkCmdPushConstants(cmd, m_gtaoPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
		0, sizeof(pc), &pc);
	vkCmdDispatch(cmd, (m_swapchainExtent.width + 15) / 16, (m_swapchainExtent.height + 15) / 16, 1);

	// 5. compute → graphics barrier
	VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
	barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);

	// 6. AO 图 → SHADER_READ_ONLY 供 fragment shader 采样
	transitionImageLayout(cmd, m_aoImage.image, VK_FORMAT_R8_UNORM,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1, 0);
	
	transitionImageLayout(cmd, m_depthImage.image, m_depthFormat,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1, 1, 0);
}


void ErisEngine::drawForwardFrame(VkCommandBuffer cmd, FrameData& frame, uint32_t imageIndex)
{
	// 1. 阴影 pass
	executeShadowPass(cmd);

	// 2. 前向渲染世界
	executeForwardPass(cmd);

	// 3. Editor UI
	executeEditorUIPass(cmd, imageIndex);
}

void ErisEngine::drawLumenFrame(VkCommandBuffer cmd,FrameData& frame, uint32_t imageIndex)
{
	// ---------------------------------------------------------
	// 【PASS 0: 阴影渲染】
	// ---------------------------------------------------------
	executeShadowPass(cmd);

	// ---------------------------------------------------------
	// 【PASS 1: Geometry Pass - 填充 GBuffer】
	// ---------------------------------------------------------
	executeGBufferPass(cmd);

	// ---------------------------------------------------------
		// [PASS 2: Compute shader - GTAO
		// ---------------------------------------------------------
	executeGTAOPass(cmd);

	// ---------------------------------------------------------
	// 【PASS 3: Lighting Pass - 最终光照与 Lumen 计算】
	// ---------------------------------------------------------
	executeLumenCompositionPass(cmd);

	// ---------------------------------------------------------
   // 【PASS 4: UI Pass - 叠加 ImGui 界面到最终交换链】
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


	// 2. 将数据写入本帧的 UBO 内存
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
	fbInfo.renderPass = m_viewportPass;
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
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1);
		transitionImageLayout(cmd, m_depthImage.image, m_depthFormat,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1);
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

	AllocatedBuffer stagingbuffer;
	vmaCreateBuffer(m_allocator, &stagingInfo, &stagingAllocInfo, &stagingbuffer.buffer, &stagingbuffer.allocation, nullptr);

	// 拷贝像素数据到暂存缓冲
	void* data;
	vmaMapMemory(m_allocator, stagingbuffer.allocation, &data);
	memcpy(data, pixels, static_cast<size_t>(imageSize));
	vmaUnmapMemory(m_allocator, stagingbuffer.allocation);
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

		vkCmdCopyBufferToImage(cmd, stagingbuffer.buffer, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

		// C. 转换布局：Transfer Destination -> Shader Read Only (准备给 Shader 用)
		transitionImageLayout(cmd, newImage.image, imageFormat, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,1);
		});

	// 5. 创建 Image View
	newImage.imageView = createImageView(newImage.image, imageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);

	// 6. 清理暂存缓冲
	vmaDestroyBuffer(m_allocator, stagingbuffer.buffer, stagingbuffer.allocation);

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

	AllocatedBuffer stagingbuffer;
	// 这里不需要传 VmaAllocationInfo 了，因为我们后面手动 Map
	if (vmaCreateBuffer(m_allocator, &stagingInfo, &stagingAllocInfo,
		&stagingbuffer.buffer, &stagingbuffer.allocation, nullptr) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create staging buffer for default texture");
	}

	// 拷贝数据
	void* data;
	vmaMapMemory(m_allocator, stagingbuffer.allocation, &data);
	memcpy(data, &whitePixel, 4);
	vmaUnmapMemory(m_allocator, stagingbuffer.allocation);

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

		vkCmdCopyBufferToImage(cmd, stagingbuffer.buffer, m_defaultTexture.image,
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
	vmaDestroyBuffer(m_allocator, stagingbuffer.buffer, stagingbuffer.allocation);

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

//void ErisEngine::drawWorld(VkCommandBuffer cmd, ErisWorld& world) {
//
//	FrameData& frame = getCurrentFrame();
//	
//	// 1. 获取通用的 View 和 Projection（一帧只需计算一次）
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
//	// 2. 遍历世界里所有的物体
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
//		// 推送当前物体的矩阵
//		vkCmdPushConstants(cmd, currentLayout, VK_SHADER_STAGE_VERTEX_BIT| VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(MeshPushConstants), &constants);
//
//
//		// --- 这里是原本遍历 mesh 的循环 ---
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
//			// 【关键】：将 C++ 的参数传给 Shader
//			constants.materialParams = glm::vec4(rough, metal, emiss, 0.0f);
//
//			// 【关键】：推入常量的 stageFlags 也要带上 FRAGMENT_BIT
//			vkCmdPushConstants(cmd, currentLayout,
//				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
//				0, sizeof(MeshPushConstants), &constants);
//
//
//			if (!isShadowPass) {
//				// 绑定描述符集 (Set 0)
//				VkDescriptorSet set_to_bind = (mesh.material && mesh.material->textureSet != VK_NULL_HANDLE)
//					? mesh.material->textureSet : m_defaultTextureSet;
//
//				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 
//					currentLayout, 1, 1, &set_to_bind, 0, nullptr);
//			}
//
//
//			// 绑定顶点和索引
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
//	// --- 3. 绘制天空盒 (放在物体之后，利用深度测试) ---
//	// ---------------------------------------------------------
//	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_skyboxPipeline);
//
//	// 绑定天空盒贴图 (Set 1)
//	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_skyboxPipelineLayout, 
//		0, 1, &frame.sceneDescriptorSet, 0, nullptr);
//
//	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_skyboxPipelineLayout,
//		1, 1, &m_skyboxDescriptorSet, 0, nullptr);
//
//	// 绑定天空盒的顶点数据 (m_skyboxMesh 应该是你之前生成的立方体)
//	VkBuffer skyBuffers[] = { m_skyboxMesh.vertexBuffer.buffer };
//	VkDeviceSize skyOffsets[] = { 0 };
//	vkCmdBindVertexBuffers(cmd, 0, 1, skyBuffers, skyOffsets);
//	vkCmdBindIndexBuffer(cmd, m_skyboxMesh.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
//
//	// 绘制 36 个索引（立方体）
//	vkCmdDrawIndexed(cmd, 36, 1, 0, 0, 0);
//
//	// ---------------------------------------------------------
//   // 2. 【新增】：渲染无限网格
//   // ---------------------------------------------------------
//	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gridPipeline);
//
//	// 只绑定 Set 0 (全局 UBO)
//	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gridPipelineLayout, 0, 1, &frame.sceneDescriptorSet, 0, nullptr);
//
//	// 因为顶点在 Shader 里，所以直接画 6 个点即可
//	vkCmdDraw(cmd, 6, 1, 0, 0);
//}

void ErisEngine::executeLumenCompositionPass(VkCommandBuffer cmd) {
	// --- 1. 配置 2 附件清除值 (视口图 + 深度) ---
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

	// --- 2. 执行全屏合成逻辑 ---
	vkCmdBeginRenderPass(cmd, &lightRpInfo, VK_SUBPASS_CONTENTS_INLINE);

		drawLumenLighting(cmd); // 绘制全屏三角形进行计算

		drawSkybox(cmd);        // 在合成后的画面上叠加天空盒

		drawGrid(cmd);          // 叠加无限网格

	vkCmdEndRenderPass(cmd);
}
void ErisEngine::executeGBufferPass(VkCommandBuffer cmd) {
	// --- 1. 显式初始化 4 个清除值 ---
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

	// --- 2. 配置开始信息 ---
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

	// --- 3. 执行绘制 ---
	vkCmdBeginRenderPass(cmd, &gbufferRpInfo, VK_SUBPASS_CONTENTS_INLINE);
	// 使用 Lumen GBuffer 专用管线绘制物体
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
		m_editor->draw(cmd); // 绘制编辑器界面	
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
	// 绑定全局环境数据 (Set 0)
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &frame.sceneDescriptorSet, 0, nullptr);

	float aspect = static_cast<float>(m_swapchainExtent.width) / static_cast<float>(m_swapchainExtent.height);
	glm::mat4 view = m_camera.getViewMatrix();
	glm::mat4 proj = m_camera.getProjectionMatrix(aspect);

	for (auto& objPtr : m_activeWorld->getObjects()) {
		RenderObject& obj = *objPtr;
		if (obj.m_pModel == nullptr) continue;

		glm::mat4 modelMat = obj.getTransformMatrix();

		// 显式逐成员赋值常量
		MeshPushConstants constants;
		constants.render_matrix = proj * view * modelMat;
		constants.model_matrix = modelMat;

		// 显式材质参数赋值
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
			// 绑定材质贴图 (Set 1)
			if (mesh.vertices.empty() || mesh.indices.empty()) continue;

			VkDescriptorSet texSet = (mesh.material && mesh.material->textureSet != VK_NULL_HANDLE)
				? mesh.material->textureSet : m_defaultTextureSet;
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 1, 1, &texSet, 0, nullptr);
			if (m_activePath == RenderPath::Forward) {
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 2, 1, &m_iblDescriptorSet, 0, nullptr);
			}
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

	// 阴影只需 Set 0 里的 sunlightProj
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipelineLayout, 0, 1, &frame.sceneDescriptorSet, 0, nullptr);

	for (auto& objPtr : m_activeWorld->getObjects()) {
		RenderObject& obj = *objPtr;
		if (obj.m_pModel == nullptr) continue;

		MeshPushConstants constants;
		constants.model_matrix = obj.getTransformMatrix();
		constants.render_matrix = constants.model_matrix; // 阴影 Shader 会自行处理 sunlightProj

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

	// 直接画 3 个顶点生成全屏三角形
	vkCmdDraw(cmd, 3, 1, 0, 0);
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
