#pragma once
#include <vulkan/vulkan.h>
#include <vector>

class PipelineBuilder
{
public:
	std::vector<VkPipelineShaderStageCreateInfo> m_shaderStages;
	std::vector<VkVertexInputBindingDescription> m_vertexBindings;
	std::vector<VkVertexInputAttributeDescription> m_vertexAttributes;
	std::vector<VkDynamicState> m_dynamicStates;
	std::vector<VkPipelineColorBlendAttachmentState> m_colorBlendAttachments;
	VkPipelineVertexInputStateCreateInfo m_vertexInputInfo;
	VkPipelineInputAssemblyStateCreateInfo m_inputAssembly;
	VkViewport m_viewport;
	VkRect2D m_scissor;
	VkPipelineRasterizationStateCreateInfo m_rasterizer;
	VkPipelineMultisampleStateCreateInfo m_multisampling;

	VkPipelineDepthStencilStateCreateInfo m_depthStencil;
	VkPipelineLayout m_pipelineLayout;
	


	VkPipeline buildPipeline(VkDevice device,VkRenderPass pass);

private:


};
