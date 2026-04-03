#include "ErisEditor.h"
#include "ErisEngine.h"

namespace fs = std::filesystem;

static PFN_vkVoidFunction Eris_ImGui_Loader(const char* function_name, void* user_data) {
    return vkGetInstanceProcAddr(*(VkInstance*)user_data, function_name);
}

static void syncToPhysics(ErisEngine* engine, RenderObject* obj) {
    if (!obj->m_physicsEnabled || obj->m_bodyID.IsInvalid()) return;

    // 1. 计算几何中心相对于原点的偏移
    glm::vec3 localCenter = (obj->m_pModel->maxBound + obj->m_pModel->minBound) * 0.5f;

    // 2. 计算当前旋转下的偏移矢量
    glm::mat4 rotMat = glm::mat4(1.0f);
    rotMat = glm::rotate(rotMat, glm::radians(obj->m_rotation.z), { 0, 0, 1 });
    rotMat = glm::rotate(rotMat, glm::radians(obj->m_rotation.y), { 0, 1, 0 });
    rotMat = glm::rotate(rotMat, glm::radians(obj->m_rotation.x), { 1, 0, 0 });
    glm::vec3 rotatedOffset = glm::vec3(rotMat * glm::vec4(localCenter, 1.0f));

    // 3. 物理中心 = 视觉原点 + 旋转后的偏移
    glm::vec3 physicsPos = obj->m_location + rotatedOffset;

    // 4. 调用你之前实现的 setTransform
    engine->getActiveWorld()->getPhysics().setTransform(obj->m_bodyID, physicsPos, obj->m_rotation);

    // 5. 顺便重置速度，防止手动移动后物体带着旧惯性乱飞
    engine->getActiveWorld()->getPhysics().resetVelocity(obj->m_bodyID);
}

// -----调用文件资源管理器找文件
static std::string OpenFileDialog(GLFWwindow* window) {
    char szFile[260] = { 0 };
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);

    // 获取 Win64 原生窗口句柄
    ofn.hwndOwner = glfwGetWin32Window(window);
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);

    // 设置过滤器：支持模型文件
    ofn.lpstrFilter = "3D Model Files (*.obj;*.fbx;*.gltf)\0*.obj;*.fbx;*.gltf\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;

    // OFN_NOCHANGEDIR 很关键，防止对话框改变程序的工作目录导致之后找不到着色器
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn) == TRUE) {
        std::string path = szFile;
        // 自动将 Win64 的反斜杠 \ 转换为通用的斜杠 /
        std::replace(path.begin(), path.end(), '\\', '/');
        return path;
    }
    return "";
}


void ErisEditor::init(ErisEngine* engine) {
    // 1. 创建池
    VkDescriptorPoolSize pool_sizes[] = { { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 } };
    VkDescriptorPoolCreateInfo pool_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = pool_sizes;
    vkCreateDescriptorPool(engine->m_device, &pool_info, nullptr, &m_imguiPool);

    // 2. 初始化上下文
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    
    ImGui::StyleColorsDark();
    set_theme(); 
    

    // 3. 初始化后端
    ImGui_ImplGlfw_InitForVulkan(engine->m_window, true);
    ImGui_ImplVulkan_LoadFunctions(VK_API_VERSION_1_0, Eris_ImGui_Loader, &engine->m_instance);

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = engine->m_instance;
    init_info.PhysicalDevice = engine->m_physicalDevice;
    init_info.Device = engine->m_device;
    init_info.QueueFamily = engine->m_graphicsQueueFamily;
    init_info.Queue = engine->m_graphicsQueue;
    init_info.DescriptorPool = m_imguiPool;
    init_info.MinImageCount = 3;
    init_info.ImageCount = 3;
    init_info.PipelineInfoMain.RenderPass = engine->m_uiRenderPass;
    init_info.PipelineInfoMain.Subpass = 0;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    // 如果开启了 Viewports，次级视口也需要配置（通常和主视口一致）
    init_info.PipelineInfoForViewports.RenderPass = engine->m_uiRenderPass;
    init_info.PipelineInfoForViewports.Subpass = 0;
    init_info.PipelineInfoForViewports.MSAASamples = VK_SAMPLE_COUNT_1_BIT;


    ImGui_ImplVulkan_Init(&init_info);
}

void ErisEditor::render_editor(ErisEngine* engine)
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    show_main_dockspace(engine);

    show_viewport(engine);    // 显示 3D 视口
    show_outliner(engine);    // 显示左侧物体大纲
    show_details(engine);     // 显示右侧属性面板
    show_content_browser(engine);   // 显示底部资源浏览器（如果有）
    show_environment_settings(engine);


    ImGui::End(); // 结束 DockSpace
    ImGui::Render();
}

void ErisEditor::draw(VkCommandBuffer cmd)
{
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

void ErisEditor::cleanup(VkDevice device)
{
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    vkDestroyDescriptorPool(device, m_imguiPool, nullptr);
}

void ErisEditor::show_main_dockspace(ErisEngine* engine)
{
    static bool opt_fullscreen = true;
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("ErisEditorDockSpace", nullptr, window_flags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    // --- 【核心：UE5 风格初始布局预设】 ---
    if (m_firstTime) {
        m_firstTime = false;

        // 1. 清除现有的布局（如果已经有 .ini 文件可以选不清理，但强制初始化建议清理）
        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

        // 2. 开始拆分空间
        // 这里的 ID 只是临时变量
        ImGuiID dock_id_main = dockspace_id;
        ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Right, 0.20f, nullptr, &dock_id_main);
        ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Left, 0.20f, nullptr, &dock_id_main);
        ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Down, 0.25f, nullptr, &dock_id_main);

        // 3. 将对应的窗口名称绑定到特定的 Dock ID 上
        // 注意：这里的字符串必须与你在 show_xxx 函数中 Begin 里的字符串完全一致！
        ImGui::DockBuilderDockWindow("Viewport", dock_id_main);
        ImGui::DockBuilderDockWindow("Outliner", dock_id_left);
        ImGui::DockBuilderDockWindow("Details", dock_id_right);
        ImGui::DockBuilderDockWindow("Environment", dock_id_right);
        ImGui::DockBuilderDockWindow("Content Browser", dock_id_bottom);

        // 4. 完成构建
        ImGui::DockBuilderFinish(dockspace_id);
    }

    if (ImGui::BeginMenuBar()) {
        // --- 核心改动：增加 Assets 菜单 ---
        if (ImGui::BeginMenu("Assets")) {
            if (ImGui::MenuItem("Import from File Explorer...")) {

                // 1. 打开原生对话框获取路径
                std::string filePath = OpenFileDialog(engine->m_window);

                if (!filePath.empty()) {
                    // 2. 调用 Engine 的加载逻辑
                    Model* newModel = engine->getOrLoadModel(filePath);

                    if (newModel && engine->m_activeWorld) {
                        // 3. 将模型放入世界
                        // 提取文件名作为物体名
                        std::string name = filePath.substr(filePath.find_last_of("\\/") + 1);
                        RenderObject* obj = engine->m_activeWorld->spawnObject(newModel, name);

                        // 4. 放在相机前方 5 米处
                        obj->m_location = engine->m_camera.m_position + engine->m_camera.m_front * 5.0f;

                        // 5. 自动选中新生成的物体
                        engine->m_selectedObject = obj;
                    }
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Exit")) { /* ... */ }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
}

void ErisEditor::show_viewport(ErisEngine* engine)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Viewport");

    ImVec2 windowSize = ImGui::GetContentRegionAvail();
    ImVec2 windowPos = ImGui::GetWindowPos(); // 窗口左上角坐标
    ImVec2 mousePos = ImGui::GetMousePos();  // 鼠标在屏幕上的绝对坐标
    
    ImVec2 vMin = ImGui::GetWindowContentRegionMin();
    ImVec2 vMax = ImGui::GetWindowContentRegionMax();

    ImVec2 viewportStart = { windowPos.x + vMin.x, windowPos.y + vMin.y };
    ImVec2 viewportSize = { vMax.x - vMin.x, vMax.y - vMin.y };

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered() && !ImGuizmo::IsUsing()) {
        ImVec2 mousePos = ImGui::GetMousePos();

        // --- 【精确转换】：计算 0.0 到 1.0 之间的相对位置 ---
        float relativeX = (mousePos.x - viewportStart.x) / viewportSize.x;
        float relativeY = (mousePos.y - viewportStart.y) / viewportSize.y;

        // 只有在 0-1 范围内才执行拾取（防止点到边框也触发）
        if (relativeX >= 0.0f && relativeX <= 1.0f && relativeY >= 0.0f && relativeY <= 1.0f) {
            RenderObject* picked = engine->pickObject(relativeX, relativeY);
            engine->m_selectedObject = picked;
        }
    }

    // 绘制离屏渲染的贴图 
    if (engine->m_viewportTextureSet != VK_NULL_HANDLE) {
        ImGui::Image((ImTextureID)engine->m_viewportTextureSet, windowSize);
    }
    // --- 【ImGuizmo 逻辑开始】 ---
    if (engine->m_selectedObject != nullptr) {
        // A. 初始化本帧 Gizmo 状态
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::BeginFrame(); // 必须调用

        // B. 设置绘图区域为当前的 ImGui 视口窗口
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(windowPos.x, windowPos.y, windowSize.x, windowSize.y);

        // C. 获取相机矩阵
        glm::mat4 view = engine->m_camera.getViewMatrix();
        glm::mat4 proj = engine->m_camera.getProjectionMatrix(windowSize.x / windowSize.y);

        // 【关键点】：ImGuizmo 内部使用的是 OpenGL 风格的投影（Y轴向上）
        // 如果你的 proj 矩阵已经包含了 Vulkan 的 Y 轴翻转，这里必须翻转回来
        proj[1][1] *= -1;

        // D. 获取并转换物体的模型矩阵
        glm::mat4 modelMatrix = engine->m_selectedObject->getTransformMatrix();

        // 设置操作模式（位移、旋转、缩放）
        static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::TRANSLATE);

        // 快捷键支持
        if (ImGui::IsWindowFocused()) {
            if (ImGui::IsKeyPressed(ImGuiKey_W)) mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
            if (ImGui::IsKeyPressed(ImGuiKey_E)) mCurrentGizmoOperation = ImGuizmo::ROTATE;
            if (ImGui::IsKeyPressed(ImGuiKey_R)) mCurrentGizmoOperation = ImGuizmo::SCALE;
        }

        // E. 核心绘制函数
        ImGuizmo::Manipulate(
            glm::value_ptr(view),
            glm::value_ptr(proj),
            mCurrentGizmoOperation,
            ImGuizmo::LOCAL,
            glm::value_ptr(modelMatrix)
        );

        // F. 反向同步：如果用户拖动了 Gizmo，更新 RenderObject 的属性
        if (ImGuizmo::IsUsing()) {
            float matrixTranslation[3], matrixRotation[3], matrixScale[3];
            ImGuizmo::DecomposeMatrixToComponents(
                glm::value_ptr(modelMatrix),
                matrixTranslation,
                matrixRotation,
                matrixScale
            );

            engine->m_selectedObject->m_location = glm::make_vec3(matrixTranslation);
            engine->m_selectedObject->m_rotation = glm::make_vec3(matrixRotation);
            engine->m_selectedObject->m_scale = glm::make_vec3(matrixScale);

            if (engine->m_selectedObject->m_physicsEnabled) {
                glm::vec3 localCenter = (engine->m_selectedObject->m_pModel->maxBound + engine->m_selectedObject->m_pModel->minBound) * 0.5f;

                glm::mat4 rotMat = glm::mat4(1.0f);
                rotMat = glm::rotate(rotMat, glm::radians(engine->m_selectedObject->m_rotation.z), { 0, 0, 1 });
                rotMat = glm::rotate(rotMat, glm::radians(engine->m_selectedObject->m_rotation.y), { 0, 1, 0 });
                rotMat = glm::rotate(rotMat, glm::radians(engine->m_selectedObject->m_rotation.x), { 1, 0, 0 });


                // 计算物理中心 = 当前原点位置 + 旋转后的偏移
                glm::vec3 rotatedOffset = glm::vec3(rotMat * glm::vec4(localCenter, 1.0f));

                engine->m_activeWorld->getPhysics().setTransform(
                    engine->m_selectedObject->m_bodyID,
                    engine->m_selectedObject->m_location + rotatedOffset,
                    engine->m_selectedObject->m_rotation
                );
            }
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

void ErisEditor::show_details(ErisEngine* engine)
{
    ImGui::Begin("Details");

    if (engine->m_selectedObject) {
        // Transform part
        RenderObject* obj = engine->m_selectedObject;

        ImGui::Text("Object Name: %s", obj->m_name.c_str());
        ImGui::Separator();

        // 绑定数据到 UI 控件
        if (ImGui::DragFloat3("Location", &obj->m_location.x, 0.1f)) {
            if (obj->m_physicsEnabled) {
                syncToPhysics(engine, obj);
            }
        }
        if (ImGui::DragFloat3("Rotation", &obj->m_location.x, 0.1f)) {
            if (obj->m_physicsEnabled) {
                syncToPhysics(engine, obj);
            }
        }
        ImGui::DragFloat3("Scale", &obj->m_scale.x, 0.01f);

        ImGui::Separator();     // ------------------------------------------------------------------
        // physics simulation part
        ImGui::Text("Physics");

        // 按钮：为选中的物体开启物理
        if (!obj->m_physicsEnabled) {
            if (ImGui::Button("Enable Physics", ImVec2(-1, 0))) {
                obj->m_physicsEnabled = true;
                glm::vec3 minB = obj->m_pModel->minBound;
                glm::vec3 maxB = obj->m_pModel->maxBound;
                // 计算该模型的包围盒大小作为碰撞形状
                glm::vec3 localCenter = (maxB + minB) * 0.5f;   // 几何中心
                glm::vec3 halfExtents = (maxB - minB) * 0.5f;   // 半长宽高

                obj->m_localCenter = localCenter;
                glm::vec3 physicsStartPos = obj->m_location + localCenter;

                obj->m_bodyID = engine->m_activeWorld->getPhysics().createBox(
                    physicsStartPos,
                    halfExtents,
                    false // false 代表动态（受重力）
                );
            }
        }
        else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "Physics Active");
            if (ImGui::Button("Disable Physics", ImVec2(-1, 0))) {

                // 1. 从 Jolt 物理世界移除
                engine->m_activeWorld->getPhysics().destroyBody(obj->m_bodyID);

                // 2. 重置标志位
                obj->m_physicsEnabled = false;

                // 3. 将 ID 设为无效
                obj->m_bodyID = JPH::BodyID();
            }
            ImGui::PopStyleColor();

            if (ImGui::Button("Reset Object", ImVec2(-1, 0))) {
                // 以后可以写把物体瞬移回原位的逻辑
                engine->m_selectedObject->resetToInitialState();
                engine->m_activeWorld->getPhysics().resetBodyState(
                    engine->m_selectedObject->m_bodyID,
                    engine->m_selectedObject->m_initialLocation,
                    engine->m_selectedObject->m_initialRotation,
                    engine->m_selectedObject->m_initialScale
                );

            }
        }

    }
    else {
        ImGui::Text("Select an object to edit.");
    }



    ImGui::End();
}

void ErisEditor::show_outliner(ErisEngine* engine) {
    ImGui::Begin("Outliner");

    if (engine->m_activeWorld) {
        // 遍历世界中的所有物体
        for (auto& objPtr : engine->m_activeWorld->getObjects()) {
            RenderObject* obj = objPtr.get();

            // 判断当前物体是否是被选中的那个
            bool isSelected = (engine->m_selectedObject == obj);

            // 如果点击了某个名字，更新 engine->m_selectedObject
            if (ImGui::Selectable(obj->m_name.c_str(), isSelected)) {
                engine->m_selectedObject = obj;
            }
        }
    }

    ImGui::End();
}

void ErisEditor::show_content_browser(ErisEngine* engine)
{
    ImGui::Begin("Content Browser");

    // 1. 显示当前路径，并提供一个“返回上级”的按钮
    ImGui::Text("Path: %s", m_currentDirectory.string().c_str());

    if (m_currentDirectory != "assets") { // 如果不在根目录，允许返回
        if (ImGui::Button(" <- Back ")) {
            m_currentDirectory = m_currentDirectory.parent_path();
        }
    }

    ImGui::Separator();

    // 2. 遍历当前目录下的所有内容
    // 使用表格式布局让其看起来更整齐
    if (ImGui::BeginTable("ContentTable", 2, ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Action");
        ImGui::TableHeadersRow();

        for (auto& directoryEntry : fs::directory_iterator(m_currentDirectory)) {
            const auto& path = directoryEntry.path();
            auto relativePath = fs::relative(path, "assets");
            std::string filenameString = path.filename().string();

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            if (directoryEntry.is_directory()) {
                // 如果是目录，显示文件夹图标
                if (ImGui::Selectable(("📁 " + filenameString).c_str())) {
                    m_currentDirectory /= path.filename(); // 点击进入文件夹
                }
            }
            else {
                // 如果是文件
                std::string ext = path.extension().string();
                if (ext == ".obj" || ext == ".fbx" || ext == ".gltf") {
                    ImGui::Text("📦 %s", filenameString.c_str());

                    ImGui::TableNextColumn();
                    // 如果是模型文件，提供“添加到世界”按钮
                    if (ImGui::Button(("Add##" + filenameString).c_str())) {

                        // 调用引擎资产库加载（逻辑与菜单导入一致）
                        Model* pModel = engine->getOrLoadModel(path.string());

                        if (pModel && engine->m_activeWorld) {
                            RenderObject* newObj = engine->m_activeWorld->spawnObject(pModel, filenameString);

                            // 放置在相机前方
                            newObj->m_location = engine->m_camera.m_position + engine->m_camera.m_front * 3.0f;
                            engine->m_selectedObject = newObj;
                        }
                    }
                }
                else {
                    // 其他文件（如贴图、mtl）只显示名称，不提供动作
                    ImGui::TextDisabled("📄 %s", filenameString.c_str());
                }
            }
        }
        ImGui::EndTable();
    }

    ImGui::End();
}

void ErisEditor::show_environment_settings(ErisEngine* engine)
{
    ImGui::Begin("Environment");

    if (ImGui::CollapsingHeader("Atmosphere", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::ColorEdit4("Ambient Color", &engine->m_sceneData.ambientColor.x);
        ImGui::ColorEdit4("Fog Color", &engine->m_sceneData.fogColor.x);
    }

    if (ImGui::CollapsingHeader("Sunlight", ImGuiTreeNodeFlags_DefaultOpen)) {
        // 拖拽调整太阳方向
        ImGui::DragFloat4("Sun Direction", &engine->m_sceneData.sunlightDir.x, 0.01f, -1.0f, 1.0f);
        ImGui::ColorEdit4("Sun Color", &engine->m_sceneData.sunlightColor.x);
    }

    if (ImGui::CollapsingHeader("Point Lights", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Active Lights: %d / 8", engine->m_sceneData.lightCount);

        if (ImGui::Button("Add Point Light") && engine->m_sceneData.lightCount < 8) {
            int i = engine->m_sceneData.lightCount;
            // 在相机位置生成一个灯
            engine->m_sceneData.pointLights[i].position = glm::vec4(engine->m_camera.m_position, 10.0f); // w = range
            engine->m_sceneData.pointLights[i].color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f); // w = intensity
            engine->m_sceneData.lightCount++;
        }

        // 遍历调节已有的点光源
        for (int i = 0; i < engine->m_sceneData.lightCount; i++) {
            ImGui::PushID(i);
            if (ImGui::TreeNode(("Light " + std::to_string(i)).c_str())) {
                ImGui::DragFloat3("Position", &engine->m_sceneData.pointLights[i].position.x, 0.1f);
                ImGui::DragFloat("Range", &engine->m_sceneData.pointLights[i].position.w, 0.1f, 0.0f, 100.0f);
                ImGui::ColorEdit3("Color", &engine->m_sceneData.pointLights[i].color.x);
                ImGui::DragFloat("Intensity", &engine->m_sceneData.pointLights[i].color.w, 0.1f, 0.0f, 100.0f);
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }

    ImGui::End();
}

void ErisEditor::set_theme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // --- [1. 基础框架设置] ---
    style.WindowRounding = 0.0f;       // 彻底去掉圆角，编辑器要硬朗
    style.ChildRounding = 0.0f;
    style.FrameRounding = 0.0f;
    style.GrabRounding = 0.0f;
    style.PopupRounding = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.TabRounding = 0.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.WindowPadding = ImVec2(8, 8);

    // --- [2. 颜色方案 - 深度木炭黑] ---

    // 背景色
    colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f); // 主窗口背景
    colors[ImGuiCol_ChildBg] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);

    // 文字
    colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);

    // 标题栏 (UE5 风格：不活跃和活跃时区分度极小)
    colors[ImGuiCol_TitleBg] = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);

    // 边框与间隔线 (很暗的灰色)
    colors[ImGuiCol_Border] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);

    // 输入框 (Frame)
    colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);

    // 选项卡 (Tabs) - 关键：UE5 的标签页是矩形的
    colors[ImGuiCol_Tab] = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);

    // 按钮
    colors[ImGuiCol_Button] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);

    // 树/大纲 (Header)
    colors[ImGuiCol_Header] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);

    // 检查 Win64 多视口同步
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        colors[ImGuiCol_WindowBg].w = 1.0f; // 强制不透明，解决之前提到的变暗问题
    }
}