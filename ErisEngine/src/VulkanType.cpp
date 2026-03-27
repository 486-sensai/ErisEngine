#include "VulkanType.h"
#include "ErisEngine.h"
#include <cstring>



void Model::calculateBounds()
{

}

void Mesh::uploadMesh(VmaAllocator allocator, ErisEngine* engine)
{
	// ---------------------------------------------------------
	// 1. 处理顶点缓冲区 (Vertex Buffer)
	// ---------------------------------------------------------
	if (vertices.empty() || indices.empty()) {
		return;
	}
	VkDeviceSize vertexBufferSize = vertices.size() * sizeof(Vertex);

	// 创建顶点暂存缓冲 (Staging Buffer - CPU 侧)
	VkBufferCreateInfo vStagingInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	vStagingInfo.size = vertexBufferSize;
	vStagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	VmaAllocationCreateInfo vStagingAllocInfo{};
	vStagingAllocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

	AllocatedBuffer vertexStaging;
	vmaCreateBuffer(allocator, &vStagingInfo, &vStagingAllocInfo,
		&vertexStaging.buffer, &vertexStaging.allocation, nullptr);

	// 拷贝数据到暂存缓冲
	void* vData;
	vmaMapMemory(allocator, vertexStaging.allocation, &vData);
	memcpy(vData, vertices.data(), (size_t)vertexBufferSize);
	vmaUnmapMemory(allocator, vertexStaging.allocation);

	// 创建真正的顶点缓冲 (GPU 侧 - 自动进销毁队列)
	vertexBuffer = engine->createBuffer(vertexBufferSize,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY);


	// ---------------------------------------------------------
	// 2. 处理索引缓冲区 (Index Buffer)
	// ---------------------------------------------------------
	VkDeviceSize indexBufferSize = indices.size() * sizeof(uint32_t);

	// 创建索引暂存缓冲 (Staging Buffer - CPU 侧)
	VkBufferCreateInfo iStagingInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	iStagingInfo.size = indexBufferSize;
	iStagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	AllocatedBuffer indexStaging;
	vmaCreateBuffer(allocator, &iStagingInfo, &vStagingAllocInfo,
		&indexStaging.buffer, &indexStaging.allocation, nullptr);

	// 拷贝数据到暂存缓冲
	void* iData;
	vmaMapMemory(allocator, indexStaging.allocation, &iData);
	memcpy(iData, indices.data(), (size_t)indexBufferSize);
	vmaUnmapMemory(allocator, indexStaging.allocation);

	// 创建真正的索引缓冲 (GPU 侧 - 自动进销毁队列)
	indexBuffer = engine->createBuffer(indexBufferSize,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY);

	// ---------------------------------------------------------
	// 3. 执行 GPU 拷贝指令 (使用 immediateSubmit)
	// ---------------------------------------------------------
	engine->immediateSubmit([&](VkCommandBuffer cmd) {
		// 拷贝顶点
		VkBufferCopy vCopyRegion{};
		vCopyRegion.size = vertexBufferSize;
		vkCmdCopyBuffer(cmd, vertexStaging.buffer, vertexBuffer.buffer, 1, &vCopyRegion);

		// 拷贝索引
		VkBufferCopy iCopyRegion{};
		iCopyRegion.size = indexBufferSize;
		vkCmdCopyBuffer(cmd, indexStaging.buffer, indexBuffer.buffer, 1, &iCopyRegion);
		});

	// ---------------------------------------------------------
	// 4. 销毁临时暂存缓冲
	// ---------------------------------------------------------
	vmaDestroyBuffer(allocator, vertexStaging.buffer, vertexStaging.allocation);
	vmaDestroyBuffer(allocator, indexStaging.buffer, indexStaging.allocation);
}

