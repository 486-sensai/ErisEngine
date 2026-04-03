#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>

#include <Mesh.h>
#include <DeletionQueue.h>

#include<vector>

class ErisEngine;

struct AllocatedBuffer {
    VkBuffer buffer{ VK_NULL_HANDLE };
    VmaAllocation allocation{ nullptr };
};

struct AllocatedImage {
    VkImage image{ VK_NULL_HANDLE };
	VkImageView imageView{ VK_NULL_HANDLE };
	VmaAllocation allocation{ nullptr };
	VkExtent3D imageExtent;
	VkFormat imageFormat;
};

struct FrameData {
    VkCommandPool m_commandPool;
    VkCommandBuffer m_mainCommandBuffer;
    VkSemaphore m_presentSemaphore;
    VkSemaphore m_renderSemaphore;
    VkFence m_renderFence;

    AllocatedBuffer sceneBuffer;
    VkDescriptorSet sceneDescriptorSet;
};

struct MeshPushConstants {
    glm::mat4 render_matrix;	// MVP
    glm::mat4 model_matrix;		// M
    glm::vec4 materialParams;	// 新增：x=roughness, y=metallic, z=emission, w=padding
};


struct Vertex {
	glm::vec3 pos;
	glm::vec3 normal;
	glm::vec3 color;
	glm::vec2 uv;

	static VkVertexInputBindingDescription getBindingDescription() {
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return bindingDescription;
	}

    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
        std::vector<VkVertexInputAttributeDescription> attributes(4); 

        // Location 0: Position
        attributes[0].binding = 0;
        attributes[0].location = 0;
        attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributes[0].offset = offsetof(Vertex, pos);

        // Location 1: Normal
        attributes[1].binding = 0;
        attributes[1].location = 1;
        attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributes[1].offset = offsetof(Vertex, normal);

        // Location 2: Color
        attributes[2].binding = 0;
        attributes[2].location = 2;
        attributes[2].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributes[2].offset = offsetof(Vertex, color);

        // Location 3: UV
        attributes[3].binding = 0;
        attributes[3].location = 3;
        attributes[3].format = VK_FORMAT_R32G32_SFLOAT;
        attributes[3].offset = offsetof(Vertex, uv);

        return attributes;
    }

};

struct Material {
    VkDescriptorSet textureSet{ VK_NULL_HANDLE }; // 该材质特有的贴图插槽
    AllocatedImage diffuseTexture;               // 该材质的贴图
    std::string name;

    float roughness = 0.5f;     // 粗糙度 (0.0 最光滑, 1.0 最粗糙)
    float metallic = 0.0f;      // 金属度 (0.0 非金属, 1.0 纯金属)
    float emission = 0.0f;      // 自发光强度
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices; 
    AllocatedBuffer vertexBuffer;
    AllocatedBuffer indexBuffer;
    Material* material{ nullptr };
    glm::vec3 minBound{ 0.0f };
    glm::vec3 maxBound{ 0.0f };

    // 辅助函数：将模型数据上传至 GPU（使用你之前的安全模式）
    void uploadMesh(VmaAllocator allocator, class ErisEngine* engine);
};

struct Model {
    std::vector<Mesh>meshes;
    std::vector<Material> materials;
    glm::vec3 minBound{ 0.0f };
    glm::vec3 maxBound{ 0.0f };

    void calculateBounds();

    void uploadModel(VmaAllocator allocator, class ErisEngine* engine) {
        for (auto& mesh : meshes) {
            mesh.uploadMesh(allocator, engine);
        }
    }
};

struct GPUPointLight {
    glm::vec4 position;
    glm::vec4 color;
};

// 我们需要考虑 Vulkan 的内存对齐（std140 布局）。在 UBO 中，数组和结构体必须对齐到 16 字节
struct GPUSceneData {
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 sunlightProj;
    glm::vec4 viewPos;
    glm::vec4 fogColor;
    glm::vec4 ambientColor;
    glm::vec4 sunlightDir;
    glm::vec4 sunlightColor;
    GPUPointLight pointLights[8];
    int lightCount;                 // 当前实际开启的灯光数
    float padding[3];               // 填充字节，保证结构体总大小是16的倍数
};



struct GBuffer {
    AllocatedImage position; // RGB: 世界坐标, A: 占位
    AllocatedImage normal;   // RGB: 法线, A: 金属度
    AllocatedImage albedo;   // RGB: 基础色, A: 粗糙度

    // 深度直接复用原有的 m_depthImage
};