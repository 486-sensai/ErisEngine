#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stb_image.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_glfw.h>

#include <iostream>
#include <stdexcept>
#include <vector>
#include <algorithm>
#include <optional>
#include <set>
#include <map>
#include <array>
#include <fstream>
#include <string>
#include <chrono>

#include "VulkanType.h"
#include "DeletionQueue.h"
#include "VulkanPipelineBuilder.h"
#include "Camera.h"
#include "ErisEditor.h"
#include "ErisWorld.h"

// 是否调用验证层
#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

const int MAX_FRAMES_IN_FLIGHT = 3;



const std::vector<const char*>validationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*>deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

// 
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);


struct QueueFamilyIndices {
	std::optional<uint32_t>graphicsFamily;
	std::optional<uint32_t>presentFamily;

	bool isComplete() {
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR>formats;
	std::vector<VkPresentModeKHR>presentModes;
};

class ErisEditor;
class ErisWorld;

class ErisEngine {
	friend class ErisEditor;
	friend class ErisWorld;

public:
	enum class RenderPath {
		Forward,
		Lumen
	};
	ErisEngine()
		:m_isInitialized(false), m_frameNumber(0), m_window(nullptr), m_selectedObject(nullptr),
		m_framebufferResized(false), m_isMousePressed(false)
	{}

	int Eris_init();
	void Eris_run();
	void Eris_cleanup();

	void cleanSwapchain();

private:
	bool m_isInitialized;
	bool m_isMousePressed; 
	bool m_framebufferResized;
	GLFWwindow* m_window;
	ECamera m_camera;
	VkViewport m_viewport;
	VkRect2D m_scissor;
	double m_lastX, m_lastY;

	RenderPath m_activePath = RenderPath::Forward;

	// vulkan core
	VkInstance m_instance;
	VkDebugUtilsMessengerEXT m_debugMessenger;
	VkSurfaceKHR m_surface;

	VkPhysicalDevice m_physicalDevice;
	VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
	VkDevice m_device;

	// VMA allocator
	VmaAllocator m_allocator;

	// swapchain
	VkSwapchainKHR m_swapchain;
	VkFormat m_swapchainImageFormat;
	std::vector<VkImage>m_swapchainImages;
	std::vector<VkImageView>m_swapchainImageViews;
	VkExtent2D m_swapchainExtent;

	VkQueue m_graphicsQueue;
	VkQueue m_presentQueue;
	uint32_t m_graphicsQueueFamily;

	VkRenderPass m_renderPass;
	VkRenderPass m_uiRenderPass;
	VkRenderPass m_viewportPass;
	VkRenderPass m_viewportForwardPass;
	std::vector<VkFramebuffer>m_frameBuffers;
	FrameData m_frames[MAX_FRAMES_IN_FLIGHT];
	uint32_t m_frameNumber;

	VkPipeline m_pipeline;
	VkPipelineLayout m_pipelineLayout;

	AllocatedImage	m_depthImage;
	VkFormat m_depthFormat;

	VkDescriptorSetLayout m_singleTextureLayout;
	VkDescriptorPool m_descriptorPool;
	VkSampler m_sampler;

	AllocatedImage m_defaultTexture;    // 1x1 白图
	VkDescriptorSet m_defaultTextureSet; // 1x1 白图对应的描述符集

	AllocatedImage m_skyboxImage;
	AllocatedImage m_irradianceMap;
	AllocatedImage m_prefilteredMap;
	VkImageView m_brdfImageView;
	VkDescriptorSetLayout m_iblDescriptorSetLayout;
	VkDescriptorSet m_iblDescriptorSet;
	VkDescriptorSet m_skyboxDescriptorSet;
	VkDescriptorSetLayout m_skyboxDescriptorSetLayout;
	VkPipeline m_skyboxPipeline;
	VkPipelineLayout m_skyboxPipelineLayout;
	VkSampler m_skyboxSampler;
	Mesh m_skyboxMesh;

	// queue deletion
	DeletionQueue m_mainDeletionQueue;

	// imgui stuff
	VkDescriptorPool m_imguiPool;
	ErisEditor* m_editor{ nullptr };
	AllocatedImage m_viewportImage;
	VkDescriptorSet m_viewportTextureSet{ VK_NULL_HANDLE };
	VkFramebuffer m_viewportFramebuffer{ VK_NULL_HANDLE };

	// World
	ErisWorld* m_activeWorld{ nullptr };
	RenderObject* m_selectedObject{ nullptr };
	std::map<std::string, Model>m_assetLibrary;

	// Light
	GPUSceneData m_sceneData;
	VkDescriptorSetLayout m_globalSetLayout;

	// Grid
	VkDescriptorSet m_gridDescriptorSet;
	VkPipeline m_gridPipeline;
	VkPipelineLayout m_gridPipelineLayout;
	VkSampler m_gridSampler;

	// Shadow
	AllocatedImage m_shadowImage;
	VkRenderPass m_shadowRenderPass;
	VkFramebuffer m_shadowFrameBuffer;
	VkPipeline m_shadowPipeline;
	VkPipelineLayout m_shadowPipelineLayout;
	VkExtent2D m_shadowExtent;

	// Lumen
	VkPipeline m_lumenLightingPipeline;
	VkPipelineLayout m_lumenLightingPipelineLayout;
	VkDescriptorSetLayout m_lumenDescriptorLayout;
	VkDescriptorSet m_lumenDescriptorSet;

	// G-Buffer
	GBuffer m_gbuffer;
	VkPipeline m_lumenGbufferPipeline;
	VkFramebuffer m_gbufferFramebuffer;
	VkPipelineLayout m_lumenGbufferPipelineLayout;

public:

	// ----------------------vulkan rendering functions-------------------
	void initVulkan();
	void initSwapchain();
	void initVma();
	void initCommands();
	void initSyncStructures(); 
	void initRenderPass();
	void initUIRenderPass();
	void initViewportPass();
	void initViewportForwardPass();
	void initForwardPipeline();
	void initSkyboxPipeline();
	void initGridPipeline();
	void initShadowPipeline();
	void initDescriptors();
	void initDescriptorPool();


	bool checkValidationLayerSupport();
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
	// void createDepthResources();

	static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
		auto app = reinterpret_cast<ErisEngine*>(glfwGetWindowUserPointer(window));
	}
	
	bool isDeviceSuitable(VkPhysicalDevice device);
	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
	void setupDebugMessenger();
	VkSampleCountFlagBits getMaxUsableSampleCount();

	void createSurface();
	void createLogicalDevice();


	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
	std::vector<const char*>getRequiredExtensions();

	void pickPhysicalDevice();
	void createInstance();

	// 检查扩展是否支持交换链
	bool checkDeviceExtensionSupport(VkPhysicalDevice device);
	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

	AllocatedBuffer createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);

	void createSwapchain();
	void recreateSwapchain();
	void createImageViews();
	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);

	void createDepthBuffer();
	void createFramebuffers();
	void createSceneBuffers();
	void updateSkyboxDescriptor();


	void initSkyboxMesh();
	void initShadowResources();

	VkFormat findDepthFormat();
	VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

	FrameData& getCurrentFrame();
	void drawFrame();
	void drawForwardFrame(VkCommandBuffer cmd, FrameData& frame, uint32_t imageIndex);
	void drawLumenFrame(VkCommandBuffer cmd, FrameData& frame, uint32_t imageIndex);


	void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);

	VkDescriptorSet createDescriptorSet(AllocatedImage& img);

	void transitionImageLayout(VkCommandBuffer cmd, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout,
		uint32_t layerCount = 1, uint32_t mipLevels = 1, uint32_t baseMipLevel = 0);


	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData
	);

	// Lumen G-Buffer
	void initGbuffer();

	void updateLumenDescriptorSet();

	void initLumenLightingPipeline();

	void initLumenGbufferPipeline();



	// ----------------------Non-rendering part------------------------

	bool loadMeshFromFile(const std::string& filename, Mesh& outMesh);

	bool loadModelFromFile(const std::string& filename, Model& outModel);

	AllocatedImage loadImageFromFile(const char* file);

	bool loadShaderModule(const char* filePath, VkShaderModule* outShaderModule);

	AllocatedImage loadCubemap(const std::vector<std::string>& faces);

	AllocatedImage loadHDRCubemap(const std::vector<std::string>& faces);

	VkImage loadBRDFLUT(VkImageView* outView);

	void loadPrefilteredMap();

	void updateForwardIBLDescriptorSet();

	void initDefaultResources();

	void initSceneData();


	// ----------------------Handle External Events------------------------
	void handleInput(float deltaTime);


	// imgui functions
	void initViewportResources(); // 创建离屏画布
	void cleanViewportResources();

	// World functions

	Model* getOrLoadModel(const std::string& path);
	void drawWorld(VkCommandBuffer cmd, ErisWorld& world);
	void drawMainGeometry(VkCommandBuffer cmd,VkPipeline pipeline, VkPipelineLayout layout);
	void drawSkybox(VkCommandBuffer cmd);
	void drawShadow(VkCommandBuffer cmd);
	void drawGrid(VkCommandBuffer cmd);
	void drawLumenLighting(VkCommandBuffer cmd);
	void executeLumenCompositionPass(VkCommandBuffer cmd);
	void executeGBufferPass(VkCommandBuffer cmd);
	void executeEditorUIPass(VkCommandBuffer cmd, uint32_t imageIndex);
	void executeShadowPass(VkCommandBuffer cmd);
	void executeForwardPass(VkCommandBuffer cmd);


	RenderObject* pickObject(float mouseX, float mouseY);
	ErisWorld* getActiveWorld() { return m_activeWorld; }


};