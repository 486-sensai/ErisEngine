#include "VulkanPipelineBuilder.h"
#include <iostream>

VkPipeline PipelineBuilder::buildPipeline(VkDevice device, VkRenderPass pass)
{
    m_vertexInputInfo.vertexBindingDescriptionCount= static_cast<uint32_t>(m_vertexBindings.size());
    m_vertexInputInfo.pVertexBindingDescriptions = m_vertexBindings.data();
    m_vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(m_vertexAttributes.size());
    m_vertexInputInfo.pVertexAttributeDescriptions = m_vertexAttributes.data();

    // 准备视口状态
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &m_viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &m_scissor;

    // 设置空的颜色混合（即：直接覆盖，不透明）
    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = static_cast<uint32_t>(m_colorBlendAttachments.size());
    colorBlending.pAttachments = m_colorBlendAttachments.data();

    // 配置动态状态信息
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(m_dynamicStates.size());
    dynamicState.pDynamicStates = m_dynamicStates.data();

    // 组装最终管线
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = static_cast<uint32_t>(m_shaderStages.size());
    pipelineInfo.pStages = m_shaderStages.data();
    pipelineInfo.pVertexInputState = &m_vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &m_inputAssembly;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &m_rasterizer;
    pipelineInfo.pMultisampleState = &m_multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDepthStencilState = &m_depthStencil;

    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = pass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    VkPipeline newPipeline;
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to build pipeline in vulkanpipeline builder!");
        return VK_NULL_HANDLE;
    }
    return newPipeline;
}


VkPipeline ComputePipelineBuilder::buildComputePipeline(VkDevice device, VkShaderModule shaderModule)
{
    VkPipelineShaderStageCreateInfo stage{};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = shaderModule;
    stage.pName = "main";

    VkComputePipelineCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    info.stage = stage;
    info.layout = m_pipelineLayout;

    VkPipeline pipeline;
    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create compute pipeline!");
    }
    return pipeline;
}