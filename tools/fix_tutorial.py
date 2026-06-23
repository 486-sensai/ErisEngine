with open('D:/Users/Alice486/source/repos/ErisEngine/ErisEngine Tutorial/01-IBL.md', 'r', encoding='utf-8') as f:
    text = f.read()

old = "### 修复实施步骤\n\n共 4 步：头文件加成员 → 新建 render pass 初始化函数 → 初始化中调用 → Forward 改用新 pass。\n\n### 操作 1/4：头文件添加新成员"
new = "### 修复实施步骤\n\n只需新建一个 render pass，framebuffer 复用现有的 `m_viewportFramebuffer`。\n\n### 操作 1/3：头文件添加新成员"

if old in text:
    text = text.replace(old, new)
    with open('D:/Users/Alice486/source/repos/ErisEngine/ErisEngine Tutorial/01-IBL.md', 'w', encoding='utf-8') as f:
        f.write(text)
    print("OK: Step 1 updated")
else:
    print("ERROR: pattern not found")

# Fix step 2 - remove framebuffer from member declaration
old2 = "VkRenderPass m_viewportForwardPass;\nVkFramebuffer m_viewportForwardFramebuffer;"
new2 = "VkRenderPass m_viewportForwardPass;"

if old2 in text:
    text = text.replace(old2, new2)
    with open('D:/Users/Alice486/source/repos/ErisEngine/ErisEngine Tutorial/01-IBL.md', 'w', encoding='utf-8') as f:
        f.write(text)
    print("OK: Step 2 - removed extra framebuffer member")

# Find and replace the initViewportForwardPass function - remove framebuffer creation
old3 = "\t// 5. 创建 framebuffer（引用同一组 image view，只是 render pass 不同）\n\tVkImageView att2[] = { m_viewportImage.imageView, m_depthImage.imageView };\n\tVkFramebufferCreateInfo fbInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };\n\tfbInfo.renderPass = m_viewportForwardPass;\n\tfbInfo.attachmentCount = 2;\n\tfbInfo.pAttachments = att2;\n\tfbInfo.width = m_swapchainExtent.width;\n\tfbInfo.height = m_swapchainExtent.height;\n\tfbInfo.layers = 1;\n\tvkCreateFramebuffer(m_device, &fbInfo, nullptr, &m_viewportForwardFramebuffer);\n\n\t// 6. 删除队列\n\tm_mainDeletionQueue.push_function([=]() {\n\t\tvkDestroyFramebuffer(m_device, m_viewportForwardFramebuffer, nullptr);\n\t\tvkDestroyRenderPass(m_device, m_viewportForwardPass, nullptr);\n\t\t});"

new3 = "\t// 5. 删除队列（不复用 framebuffer，直接用 m_viewportFramebuffer）\n\tm_mainDeletionQueue.push_function([=]() {\n\t\tvkDestroyRenderPass(m_device, m_viewportForwardPass, nullptr);\n\t\t});"

if old3 in text:
    text = text.replace(old3, new3)
    with open('D:/Users/Alice486/source/repos/ErisEngine/ErisEngine Tutorial/01-IBL.md', 'w', encoding='utf-8') as f:
        f.write(text)
    print("OK: Step 3 - removed framebuffer creation from function")

# Fix step 4 - remove framebuffer reference
old4 = "sceneRpInfo.renderPass = m_viewportForwardPass;\nsceneRpInfo.framebuffer = m_viewportForwardFramebuffer;"
new4 = "sceneRpInfo.renderPass = m_viewportForwardPass;\nsceneRpInfo.framebuffer = m_viewportFramebuffer; // 复用"

if old4 in text:
    text = text.replace(old4, new4)
    with open('D:/Users/Alice486/source/repos/ErisEngine/ErisEngine Tutorial/01-IBL.md', 'w', encoding='utf-8') as f:
        f.write(text)
    print("OK: Step 4 - fixed framebuffer reference")

# Remove operation 3/4 (init call) and merge into step 3
old5 = "### 操作 3/4：在 Eris_init() 中调用\n\n**文件**：`ErisEngine/src/ErisEngine.cpp`\n\n找到 `initViewportPass();` 这一行，在其下方添加：\n```cpp\ninitViewportForwardPass();\n```"
new5 = ""

if old5 in text:
    text = text.replace(old5, new5)
    with open('D:/Users/Alice486/source/repos/ErisEngine/ErisEngine Tutorial/01-IBL.md', 'w', encoding='utf-8') as f:
        f.write(text)
    print("OK: Removed redundant step")
