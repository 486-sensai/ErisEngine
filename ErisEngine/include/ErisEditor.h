#pragma once

#include "VulkanType.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_glfw.h>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#endif

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>


#include <iostream>
#include <filesystem>
#include <algorithm>


class ErisEngine;

class ErisEditor {
public:
    friend class ErisEngine;
    void uploadFonts(ErisEngine* engine);
    void init(ErisEngine* engine);
    void render_editor(ErisEngine* engine); // 编写面板逻辑
    void draw(VkCommandBuffer cmd);    // 真正的 Vulkan 绘制指令
    void cleanup(VkDevice device);

private:
    VkDescriptorPool m_imguiPool;
    std::filesystem::path m_currentDirectory = "assets";

    bool m_firstTime = true;

    // 面板私有函数
    void show_main_dockspace(ErisEngine* engine);
    void show_viewport(ErisEngine* engine);
    void show_details(ErisEngine* engine);
    void show_outliner(ErisEngine* engine);
    void show_content_browser(ErisEngine* engine);
    void show_environment_settings(ErisEngine* engine);

    void set_theme(); // UE5 配色
};