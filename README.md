# ErisEngine

基于 `Vulkan + GLFW + ImGui` 的实时渲染与编辑器原型项目，包含基础场景编辑、模型导入、物理模拟、阴影与 Lumen 风格的 G-Buffer 光照流程。

## 项目特性

- Vulkan 渲染管线（含交换链、深度、描述符、多帧并行）
- 双渲染路径基础结构（Forward / Lumen）
- Shadow Pass（阴影贴图）
- G-Buffer + Lighting Composition（Lumen 路径）
- Skybox 与无限网格
- ImGui 编辑器界面（Docking、多面板布局）
- 场景物体选择（Ray-AABB Picking）
- 基础物理集成（Jolt Physics）
- 资源浏览器 + 文件导入（支持 `.obj/.fbx/.gltf`）

## 目录结构

```text
ErisEngine/
├─ ErisEngine.sln
├─ packages/                      # NuGet 包（Assimp）
└─ ErisEngine/
   ├─ include/                    # 头文件
   ├─ src/                        # 源码
   ├─ shaders/                    # GLSL 与 SPIR-V
   ├─ assets/                     # 模型/贴图资源
   └─ third_party/                # 第三方依赖（VulkanSDK, glfw, imgui, Jolt, Assimp等）
```

## 依赖环境

建议平台：`Windows x64`  
建议工具链：`Visual Studio 2022 (v143) + C++17`

项目中实际使用/引用到的依赖：

- Vulkan SDK（项目内 `third_party/VulkanSDK`）
- GLFW 3.4（项目内 `third_party/glfw-3.4.bin.WIN64`）
- ImGui docking 分支 + ImGuizmo
- Jolt Physics
- Assimp（NuGet）
- GLM
- stb_image
- VMA（Vulkan Memory Allocator）

## 构建与运行（推荐：Visual Studio）

1. 打开解决方案：
   - `ErisEngine/ErisEngine.sln`
2. 选择配置：
   - `Debug | x64`（推荐先用这个）
3. 恢复 NuGet 包（如果首次打开有缺包提示）：
   - 在 VS 中执行 Restore
4. 构建并运行：
   - 启动项目 `ErisEngine`

> 说明：当前 `CMakeLists.txt` 为较早版本，只包含部分源文件和链接项；完整工程配置以 `.sln/.vcxproj` 为准。

## 着色器编译

在 `ErisEngine/ErisEngine/shaders` 目录下执行：

```bat
compile.bat
```

该脚本会调用 `glslangValidator.exe` 编译以下着色器（含 Lumen 子目录）到 `.spv`。

## 编辑器与操作

- 相机移动：`W / A / S / D`
- 视角旋转：按住鼠标右键拖动
- 物体选择：在 Viewport 内左键点击
- Gizmo 切换：`W`(Translate) / `E`(Rotate) / `R`(Scale)

编辑器面板包含：

- Viewport
- Outliner
- Details
- Content Browser
- Environment

可通过菜单 `Assets -> Import from File Explorer...` 导入模型并放入场景。

## 当前默认资源

项目启动后会尝试加载：

- `assets/sportsCar/sportsCar.obj`
- `assets/ground/churchfloor.obj`
- `assets/skybox/*`

请确保运行目录下资源路径有效，否则会出现模型/贴图加载失败。

## 后续建议

- 补齐并维护 `CMakeLists.txt`，与 VS 工程保持一致
- 增加跨平台构建说明（Linux/macOS）
- 增加渲染路径切换 UI 与性能统计面板
- 增加单元测试/集成测试与 CI

---

欢迎继续完善 ErisEngine。

