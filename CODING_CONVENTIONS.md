# ErisEngine 编码规范

> 最后更新: 2026-06-18
> 用途: 所有修改代码前应参考此文件，保持风格一致。

---

## 1. 命名规范

### 1.1 文件命名

| 类型 | 风格 | 示例 |
|------|------|------|
| C++ 头文件 (.h) | PascalCase | `ErisEngine.h`, `VulkanPipelineBuilder.h` |
| C++ 源文件 (.cpp) | PascalCase | `ErisEngine.cpp`, `VulkanPipelineBuilder.cpp` |
| GLSL 着色器 (.vert/.frag) | 全小写 + 点分隔 | `triangle.vert`, `grid.frag`, `skybox.frag` |
| 编译后 SPIR-V (.spv) | snake_case | `sky_vert.spv`, `gbuffer_frag.spv` |

### 1.2 类型命名 (class / struct / enum)

- 一律使用 **PascalCase**。
- 枚举统一使用 `enum class`（禁止无作用域枚举）。

```cpp
class ErisEngine {};
struct QueueFamilyIndices {};
enum class RenderPath { Forward, Lumen };
```

### 1.3 函数 / 方法命名

项目内存在三种风格混用，**已有代码请保持原风格，新增代码优先使用 PascalCase**：

| 风格 | 使用场景 | 示例 |
|------|----------|------|
| PascalCase | 引擎核心类方法 | `initVulkan()`, `createSwapchain()`, `drawFrame()` |
| snake_case | Editor 类方法、工具函数 | `render_editor()`, `push_function()`, `syncToPhysics()` |
| camelCase | 少量转换辅助函数 | `aiMatrix4x4ToGlm()`, `getOrLoadModel()` |

着色器入口固定为 `void main()`。

### 1.4 变量命名

| 作用域 | 风格 | 示例 |
|--------|------|------|
| 成员变量 | `m_` 前缀 + PascalCase | `m_isInitialized`, `m_frameNumber`, `m_selectedObject` |
| 局部变量 | camelCase（主要） | `surfaceFormat`, `createInfo`, `imageCount` |
| 局部变量 | snake_case（少数 Vulkan 教程遗留） | `vStagingInfo`, `vertexStaging` |
| 函数参数 | camelCase | `device`, `filePath`, `deltaTime` |
| 全局常量 (文件域) | PascalCase | `MAX_FRAMES_IN_FLIGHT` |

### 1.5 命名空间

- PascalCase。

```cpp
namespace Layers { constexpr JPH::ObjectField NON_MOVING = 0; }
namespace fs = std::filesystem;
```

### 1.6 宏

- 全大写 + 下划线分隔（`UPPER_SNAKE_CASE`）。

```cpp
#define GLFW_INCLUDE_VULKAN
#define STB_IMAGE_IMPLEMENTATION
#define VMA_IMPLEMENTATION
```

### 1.7 Include Guard

- **一律使用 `#pragma once`**，不使用 `#ifndef` 宏保护。

---

## 2. 代码风格

### 2.1 缩进

> **注意：** 项目内存在不一致，修改文件时遵循该文件原有风格。

| 文件 | 缩进方式 |
|------|----------|
| `ErisEngine.cpp` | **Tab** |
| `main.cpp` | **Tab** |
| `DeletionQueue.cpp` | **Tab** |
| `VulkanPipelineBuilder.cpp` | **Tab** |
| 其余 .cpp 和所有 .h | **4 空格** |

**新增文件统一使用 4 空格**。

### 2.2 花括号

- **Allman 风格**（括号独占一行）：

```cpp
void ErisEngine::createSwapchain()
{
    if (condition)
    {
        // body
    }
}
```

- 仅有极短的类/结构体内联函数可同行：

```cpp
bool isComplete() {
    return graphicsFamily.has_value() && presentFamily.has_value();
}
```

### 2.3 空格

- 二元运算符两侧各一个空格（`=`, `+`, `==`, `&&` 等）。
- 指针 `*` 和引用 `&` 紧贴类型（左侧）：`const std::string& name`, `Model* model`。
- 逗号后加空格：`func(a, b, c)`。

### 2.4 注释

- **使用中文**书写注释。
- 统一使用 `//` 风格，不使用 `/* */` 块注释。
- 分隔性注释使用 `// ---- 标题 ----`。

---

## 3. 文件组织

### 3.1 目录结构

```
ErisEngine/
  CMakeLists.txt
  ErisEngine.vcxproj    # 主构建文件
  include/              # 所有 .h 头文件
  src/                  # 所有 .cpp 源文件
  shaders/              # GLSL 着色器 (.vert/.frag) + 编译产物 (.spv)
    Lumen/              # Lumen 相关着色器
  assets/               # 模型 / 纹理 / 天空盒
  third_party/          # 第三方依赖
```

### 3.2 头文件与源文件

- 头文件放 `include/`，源文件放 `src/`。
- 每个 `.h` 尽可能对应一个 `.cpp`（除非全部是内联实现）。
- 头文件禁止包含 `.cpp` 实现。

### 3.3 Include 顺序

```
1. 本模块头文件        // #include "ErisEngine.h"
2. C++ 标准库          // <vector>, <string>, <algorithm>
3. 第三方库            // <GLFW/glfw3.h>, <vulkan/vulkan.h>, <imgui.h>
4. 项目其他头文件      // "VulkanType.h", "Camera.h"
```

每组内部大致按字母序排列。

---

## 4. C++ 语言特性使用规范

### 4.1 const / constexpr

- 不应修改的局部变量加 `const`。
- 不修改成员变量的成员函数加 `const`。
- 参数尽量使用 `const&` 传递大对象。
- 编译期常量使用 `constexpr`。

### 4.2 auto

- **适度使用**，主要用于：
  - 迭代器 / 范围 for：`for (auto& obj : m_objects)`
  - 类型转换结果：`auto app = reinterpret_cast<ErisEngine*>(...)`
  - lambda 表达式
- 一般变量优先写出显式类型。

### 4.3 指针与引用

- 非拥有指针使用**原始指针**：`Model* model`。
- 空指针使用 `nullptr`（不用 `NULL`）。
- `*` 和 `&` 紧贴类型名（左侧）。

### 4.4 智能指针

- 所有权用 `std::unique_ptr`：`std::vector<std::unique_ptr<RenderObject>> m_objects`。
- **禁止使用 `std::shared_ptr`**（项目目前不使用）。
- 非拥有观察引用一律用原始指针。

### 4.5 RAII 模式

- 使用 **DeletionQueue** 模式管理 Vulkan 资源生命周期：

```cpp
m_mainDeletionQueue.push_function([=]() { vkDestroyX(m_device, handle, nullptr); });
m_mainDeletionQueue.flush();  // 析构时统一清理
```

- 不使用基于作用域的 RAII 包装器来管理 Vulkan 对象。

### 4.6 错误处理

- 不可恢复错误使用异常：`throw std::runtime_error("...")`。
- 外部库（Vulkan）返回值检查：`if (result != VK_SUCCESS)`。
- 可恢复操作返回 `bool`。

---

## 5. 项目特有模式

### 5.1 Vulkan 层约定

- 使用 **Vulkan C API**（非 `vulkan.hpp` C++ 包装）。
- 通过 **volk** 动态加载 Vulkan 函数。
- 通过 **VMA (Vulkan Memory Allocator)** 管理 GPU 内存。
- 显式填充 `Vk*CreateInfo` 结构体（大括号初始化第一个成员）。
- Pipeline 使用 **Builder 模式** 构建：

```cpp
PipelineBuilder builder;
builder.vertexInputInfo = ...;
builder.rasterizer = ...;
VkPipeline pipeline = builder.buildPipeline(device, pass);
```

- Descriptor Set 布局：`set=0` 为全局场景数据，`set=1` 为纹理。
- 使用 Push Constants 传递每对象变换数据。
- GPU 上传使用 `immediateSubmit` 单发队列模式。

### 5.2 着色器约定

- `#version 450` + `#extension GL_KHR_vulkan_glsl : enable`
- Push constants 统一命名：`layout(push_constant) uniform constants { ... } PushConstants;`
- 使用 `glslangValidator.exe` 离线编译（通过 `compile.bat`）。

### 5.3 日志

- 无日志框架，直接使用 `std::cerr`（错误/校验消息）和 `std::cout`（调试信息）。
- 新增代码同理，无需引入日志库。

### 5.4 构建系统

- 主构建文件：`ErisEngine.vcxproj`（Visual Studio）。
- 辅构建文件：`CMakeLists.txt`（可能不完整）。
- C++ 标准：C++17。

---

## 6. 重要说明

- **规范执行优先级**：修改已有文件时，优先遵循该文件既有风格（尤其是缩进和函数命名风格），再参考本文件。
- **新增文件**：缩进 4 空格，Allman 花括号，PascalCase 类型/函数/文件命名，`#pragma once`。
- **着色器**：文件全小写，入口 `main()`，PascalCase 内部结构体。
- **不引入**：`std::shared_ptr`、`vulkan.hpp`、`#ifndef` 头文件保护、`/* */` 块注释。
