# ErisEngine 编码规范

> 最后更新: 2026-06-18
> 用途: 所有修改代码前应参考此文件，保持风格一致。

---

## 1. 命名规范

### 1.1 文件命名


| 类型                      | 风格            | 示例                                          |
| ------------------------- | --------------- | --------------------------------------------- |
| C++ 头文件 (.h)           | PascalCase      | `ErisEngine.h`, `VulkanPipelineBuilder.h`     |
| C++ 源文件 (.cpp)         | PascalCase      | `ErisEngine.cpp`, `VulkanPipelineBuilder.cpp` |
| GLSL 着色器 (.vert/.frag) | 全小写 + 点分隔 | `triangle.vert`, `grid.frag`, `skybox.frag`   |
| 编译后 SPIR-V (.spv)      | snake_case      | `sky_vert.spv`, `gbuffer_frag.spv`            |

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


| 风格       | 使用场景                | 示例                                                    |
| ---------- | ----------------------- | ------------------------------------------------------- |
| PascalCase | 引擎核心类方法          | `initVulkan()`, `createSwapchain()`, `drawFrame()`      |
| snake_case | Editor 类方法、工具函数 | `render_editor()`, `push_function()`, `syncToPhysics()` |
| camelCase  | 少量转换辅助函数        | `aiMatrix4x4ToGlm()`, `getOrLoadModel()`                |

着色器入口固定为 `void main()`。

### 1.4 变量命名


| 作用域            | 风格                               | 示例                                                   |
| ----------------- | ---------------------------------- | ------------------------------------------------------ |
| 成员变量          | `m_` 前缀 + PascalCase             | `m_isInitialized`, `m_frameNumber`, `m_selectedObject` |
| 局部变量          | camelCase（主要）                  | `surfaceFormat`, `createInfo`, `imageCount`            |
| 局部变量          | snake_case（少数 Vulkan 教程遗留） | `vStagingInfo`, `vertexStaging`                        |
| 函数参数          | camelCase                          | `device`, `filePath`, `deltaTime`                      |
| 全局常量 (文件域) | PascalCase                         | `MAX_FRAMES_IN_FLIGHT`                                 |

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


| 文件                        | 缩进方式   |
| --------------------------- | ---------- |
| `ErisEngine.cpp`            | **Tab**    |
| `main.cpp`                  | **Tab**    |
| `DeletionQueue.cpp`         | **Tab**    |
| `VulkanPipelineBuilder.cpp` | **Tab**    |
| 其余 .cpp 和所有 .h         | **4 空格** |

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

## 3. 文件编码

### 3.1 核心原则 — 铁律

**含中文注释或中文字符串的文件，一律使用 GBK 编码保存。**
**纯 ASCII（无中文）的文件可以使用 UTF-8（无 BOM）。**

> ⚠️ 这是**铁律（IRON RULE）**。任何时候修改代码，必须确保编辑器的"保存编码"设置为 GBK（或中文 Windows 的 ANSI），否则你的 AI 助手/编辑器可能以 UTF-8 写入文件，导致中文注释和字符串全部乱码。

**对本 AI 助手的硬性约束（历史教训）：**

1. **不得使用 `Write` 工具写入含中文的 GBK 文件。** `Write` 工具输出 UTF-8，直接破坏 GBK 中文。
2. **不得使用 `Edit` 工具修改含中文的 GBK 文件。** `Edit` 工具替换字符串时混合 UTF-8 字节到 GBK 流中，导致文件永久损坏且不可逆。
3. **修改 GBK 文件的唯一安全方式：**
   - 改纯英文/ASCII 内容 → 用 `sed -i`（字节级替换，不碰中文区域）
   - 必须写含中文的文件 → 用 `python3` 以 `'wb'` 模式写入 `.encode('gbk')` 后的字节
   - 必须重构整个文件 → 先从 git 恢复原版，再用 sed/python 做小改
4. **如何判断文件编码：** 用 `file` 命令：`file xxx.cpp` → `ISO-8859 text` 就是 GBK（`file` 无法从单字节编码中区分 GBK）。

### 3.2 编码分布（已有文件）


| 编码               | 适用场景                             | 涉及文件                                                                                                                                                                                                                                                                                                              |
| ------------------ | ------------------------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **GBK**            | 含中文注释/字符串的 C++ 文件、着色器 | `ErisEngine.h`, `ErisEngine.cpp`, `Camera.h`, `ErisEditor.h`, `ErisPhysics.h/cpp`, `ErisWorld.h/cpp`, `LayoutTransition.h`, `RenderObject.h/cpp`, `VulkanPipelineBuilder.cpp`, `VulkanType.h/cpp`, `triangle.vert/frag`, `grid.vert/frag`, `shadow.vert`, `skybox.vert`, `Lumen/gbuffer.vert/frag`, `Lumen/main.frag` |
| **UTF-8 (无 BOM)** | 纯 ASCII 文件（无中文）              | `DeletionQueue.h/cpp`, `Mesh.h`, `VulkanInitializers.h/cpp`, `VulkanPipelineBuilder.h`, `StbImageImplementation.cpp`, `VmaUsage.cpp`, `main.cpp`, `compile.bat`, `Lumen/main.vert`, `skybox.frag`, `README.md`                                                                                                        |
| **UTF-8 with BOM** | 特例                                 | `ErisEditor.cpp`（不建议新增 UTF-8 BOM 文件）                                                                                                                                                                                                                                                                         |

### 3.3 判断方法与操作指引

- **如何判断文件编码**：用 VS Code 或 Notepad++ 打开，查看右下角编码指示器。
- **VS Code 设置**：在 `.vscode/settings.json` 中设置 `"files.encoding": "gbk"` 以确保新建/保存中文文件时使用 GBK。
- **新增含中文的文件**：手动选择「以 GBK 编码保存」；或在文件最开头写一条中文注释后立即改编码为 GBK 保存。
- **不小心用 UTF-8 保存了中文文件**：打开后另存为 GBK（ANSI），中文即可恢复。
- **着色器文件**（`shaders/*.vert/frag`）：同样遵循此规则——含中文注释的必须 GBK。

---

## 4. 文件组织

### 4.1 目录结构

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

### 4.2 头文件与源文件

- 头文件放 `include/`，源文件放 `src/`。
- 每个 `.h` 尽可能对应一个 `.cpp`（除非全部是内联实现）。
- 头文件禁止包含 `.cpp` 实现。

### 4.3 Include 顺序

```
1. 本模块头文件        // #include "ErisEngine.h"
2. C++ 标准库          // <vector>, <string>, <algorithm>
3. 第三方库            // <GLFW/glfw3.h>, <vulkan/vulkan.h>, <imgui.h>
4. 项目其他头文件      // "VulkanType.h", "Camera.h"
```

每组内部大致按字母序排列。

---

## 5. C++ 语言特性使用规范

### 5.1 const / constexpr

- 不应修改的局部变量加 `const`。
- 不修改成员变量的成员函数加 `const`。
- 参数尽量使用 `const&` 传递大对象。
- 编译期常量使用 `constexpr`。

### 5.2 auto

- **适度使用**，主要用于：
  - 迭代器 / 范围 for：`for (auto& obj : m_objects)`
  - 类型转换结果：`auto app = reinterpret_cast<ErisEngine*>(...)`
  - lambda 表达式
- 一般变量优先写出显式类型。

### 5.3 指针与引用

- 非拥有指针使用**原始指针**：`Model* model`。
- 空指针使用 `nullptr`（不用 `NULL`）。
- `*` 和 `&` 紧贴类型名（左侧）。

### 5.4 智能指针

- 所有权用 `std::unique_ptr`：`std::vector<std::unique_ptr<RenderObject>> m_objects`。
- **可以使用 `std::shared_ptr`**（项目目前不使用）。
- 非拥有观察引用一律用原始指针。

### 5.5 RAII 模式

- 使用 **DeletionQueue** 模式管理 Vulkan 资源生命周期：

```cpp
m_mainDeletionQueue.push_function([=]() { vkDestroyX(m_device, handle, nullptr); });
m_mainDeletionQueue.flush();  // 析构时统一清理
```

- 不使用基于作用域的 RAII 包装器来管理 Vulkan 对象。

### 5.6 错误处理

- 不可恢复错误使用异常：`throw std::runtime_error("...")`。
- 外部库（Vulkan）返回值检查：`if (result != VK_SUCCESS)`。
- 可恢复操作返回 `bool`。

---

## 6. 项目特有模式

### 6.1 Vulkan 层约定

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

### 6.2 着色器约定

- `#version 450` + `#extension GL_KHR_vulkan_glsl : enable` 有需要会添加新的...
- Push constants 统一命名：`layout(push_constant) uniform constants { ... } PushConstants;`
- 使用 `glslangValidator.exe` 离线编译（通过 `compile.bat`）。

### 6.3 日志

- 无日志框架，直接使用 `std::cerr`（错误/校验消息）和 `std::cout`（调试信息）。
- 新增代码同理，无需引入日志库。

### 6.4 构建系统与依赖版本


| 依赖                              | 版本                    | 说明                                                                         |
| --------------------------------- | ----------------------- | ---------------------------------------------------------------------------- |
| **Vulkan SDK**                    | **1.4.335.0**           | SDK 版本。                                                                   |
| **Vulkan API 版本**               | `VK_API_VERSION_1_4`    | 已升级。`appInfo.apiVersion` 和 `allocatorInfo.vulkanApiVersion`均使用 1.4。 |
| **Vulkan 头文件**                 | `vulkan_core.h` 1.4.335 | `VK_HEADER_VERSION_COMPLETE = VK_MAKE_API_VERSION(0, 1, 4, 335)`             |
| **VMA (Vulkan Memory Allocator)** | 3.3.0                   | 位于 SDK 内`vma/vk_mem_alloc.h`                                              |
| **volk**                          | 最新 (zeux/volk)        | Vulkan 动态加载，位于`Volk/volk.h`                                           |
| **GLFW**                          | 3.4                     | `glfw-3.4.bin.WIN64`，静态链接 `glfw3.lib`                                   |
| **GLM**                           | 1.0.2                   | 随 Vulkan SDK 分发                                                           |
| **Jolt Physics**                  | 5.5.1                   | 静态链接`Jolt.lib`                                                           |
| **ImGui**                         | 1.92.7 WIP              | docking 分支                                                                 |
| **ImGuizmo**                      | 最新 (master)           | 集成于 ImGui 上层                                                            |
| **Assimp**                        | 3.0.0                   | NuGet 包导入 (`Assimp.redist.3.0.0`)                                         |
| **stb_image**                     | 最新 (单文件)           | 用于纹理加载                                                                 |

**构建工具链**：


| 工具                 | 版本                                 |
| -------------------- | ------------------------------------ |
| **Visual Studio**    | 2022 (Platform Toolset v143)         |
| **Windows SDK**      | 10.0                                 |
| **C++ 标准**         | C++17 (`stdcpp17`)                   |
| **CMake**            | ≥ 3.15（辅构建系统）                |
| **glslangValidator** | 随 Vulkan SDK 分发（着色器离线编译） |

**主构建文件**：`ErisEngine.vcxproj`（Visual Studio）
**辅构建文件**：`CMakeLists.txt`（可能不完整，以 vcxproj 为准）

---

## 7. 开发工作流

### 7.1 规范执行优先级

修改已有文件时，优先遵循该文件既有风格（尤其是缩进和函数命名风格），再参考本文件。

- **新增文件**：缩进 4 空格，Allman 花括号，PascalCase 类型/函数/文件命名，`#pragma once`。
- **着色器**：文件全小写，入口 `main()`，PascalCase 内部结构体。
- **不引入**：`std::shared_ptr`、`vulkan.hpp`、`#ifndef` 头文件保护、`/* */` 块注释。

### 7.2 功能开发流程

所有新功能开发必须遵循以下三阶段：

#### 阶段一：方案规划

- 在 `ErisEngine Plans\` 目录中编写方案文档（.md），说明：
  - 功能目标
  - 技术方案概述
  - 涉及的文件清单
  - 前置依赖
  - 关键任务 checklist

#### 阶段二：实现步骤（Tutorial）

- 在 `ErisEngine Tutorial\` 目录中编写逐步实施方案（.md），内容包含：
  - 每一步的具体操作（代码位置、修改内容）
  - 涉及的 API 说明和参数选择依据
  - 可参考的网上资料链接（博客、文档、论文等）

#### 阶段三：工作总结

功能落地后，在 `ErisEngine Worklog\` 目录中创建工作总结文档（.md），命名格式为 `XX-功能名.md`，内容包含：

```
---

## 工作总结

### 问题描述
[要解决的问题是什么，预期要达到什么效果]

### 技术选型依据
[为什么选择这个方案而不是其他方案，各方案对比]

### 实现手段概述
[用什么技术手段实现，涉及的关键 API/算法]

### 遇到的难题
[实现过程中遇到的主要困难]

### 解决办法
[如何解决这些困难]

### 参考来源
[参考的文档、博客、论文等]
```

### 7.3 目录结构约定

```
D:\Users\Alice486\source\repos\ErisEngine\
├── ErisEngine\                 # Git 仓库根目录
│   ├── ErisEngine\             # 源码
│   ├── shaders\                # 着色器
│   └── CODING_CONVENTIONS.md   # 本文件
├── ErisEngine Plans\           # 方案规划文档（在 git 之外）
│   └── README.md               # 路线图总览
├── ErisEngine Tutorial\        # 实现步骤（在 git 之外）
│   ├── 01-IBL.md
│   ├── 02-HBAO.md
│   └── ...
└── ErisEngine Worklog\         # 工作总结（在 git 之外）
    ├── 01-IBL.md
    ├── 02-HBAO.md
    └── ...
```

### 7.4 协助开发标准流程

每个功能被拆解成若干个 Step，每个 Step 遵循以下 **6 步循环**：

> **日志规则**：每个 Phase 建立一份工作总结（`ErisEngine Worklog\XX-功能名.md`），
> 每完成一个 Step 后追加该 Step 的日志内容（问题描述、选型依据、难题、解决、参考）。
> Step 3 中用户提出的技术 Q&A 也应一并记入日志。
>
> **日志质量标准**（面试导向）：
> 工作日志要写到"面试官问到这个功能时能直接拿出来当答案"的程度。
> 具体标准：
> 1. 事无巨细：不仅记录"做了什么"，还要记录"为什么这么做"、"为什么不做另一个方案"
> 2. 技术细节：包含具体的 API 参数选择、数据结构设计、兼容性考量
> 3. 踩坑记录：遇到的错误、排查思路、根因分析、修复方法
> 4. 通俗易懂：假设读者了解基础概念但不知道本项目细节，用例子和类比说明复杂问题
> 5. 面试友好：每个技术决策都要能解释得清楚——"为什么选 A 不选 B"的答案是面试高频题

> **运行时错误处理**：编译/运行时报错时，遵循以下流程：
>
> 1. **控制台逐步调试**：AI 引导用户在控制台中一步步修改代码、重新编译运行，直到正确为止
> 2. **更新 Tutorial**：将最终正确的代码更新到 Tutorial 中对应 Step（替换错误代码），保证教程始终可复现
> 3. **记录 Worklog**：清楚描述报错信息、根因分析、修改方法、涉及的知识点（面试级标准）
> 4. 确认无误后 → 进入下一步

```
Step 1 ── 方案规划（AI 写，用户审）
  → AI 编写方案文档 → 用户审查 → 反馈修改 → 确认
  → **方案文档必须包含**：该技术解决什么问题、预期效果、实现手段、选择此方案的原因（为什么选 A 不选 B）

Step 2 ── 教程编写（AI 写，用户审）
  → AI 编写逐步实施文档（含完整代码）→ 用户审查 → 确认
  → **着色器代码必须配文字说明**：每段 GLSL 要写清楚"这段在干什么"、"对应的数学/物理原理是什么"（如：降采样为什么用 4-tap box filter、上采样为什么加回而不是覆盖）

Step 3 ── 编码实施（用户主导，AI 协助）
  → 用户根据 Tutorial 亲手敲代码
  → AI 提供指引、解释、答疑
  → 用户可随时要求 AI 检查当前进度

Step 4 ── 代码审查（AI）
  → AI 审查 diff，检查：
    ・逻辑正确性
    ・编码规范一致性（命名、缩进、GBK 编码等）
    ・与现有代码的兼容性
    ・中文注释是否乱码
  → **回应"为什么"的规则**：当用户问"为什么这样做"时，是在要求解释方案对比理由（为什么选 A 不选 B），不是要求改方案。应先解释原因，再问是否需要调整，而不是直接改。
  → **审查规则**：如果怀疑某处可能出错，必须顺着代码逻辑实际验证（读相关变量定义、追函数调用链、核对参数值），而非仅凭猜测向用户报告"可能有问题"
  → 有问题 → 告知用户修正 → 重新审查
  → 确认无误 → 进入下一步

Step 5 ── 构建 & 测试（用户）
  → 用户本地构建（VS）
  → 运行测试 / 目测效果
  → 无错误 → 进入下一步

Step 6 ── 提交 & 日志（用户发令，AI 执行）
  → 用户下达提交指令
  → AI 执行 git add / commit / push / PR
  → AI 编写工作总结日志到 ErisEngine Worklog\
```

每个 Step 走完后，进入 Tutorial 中的下一个 Step，重复 Step 3-6。

### 7.5 代码结构原则

#### 单一职责

- **每个函数做一件事**。不要往现有函数中硬塞长串代码，应新建独立函数。
  - 例如：描述符写入应封装为 `updateXxxDescriptorSet()`，不要写在 `Eris_init()` 或 `drawFrame()` 中。
- **不同功能板块应拆分到不同 .h/.cpp 文件**，而不是全部堆积在 `ErisEngine.cpp`。
  - 渲染管线相关优先放入 `VulkanPipelineBuilder.h/cpp`
  - 描述符管理使用独立函数或独立文件
  - Vulkan 类型定义放入 `VulkanType.h`
- 函数职责不清时，优先拆函数，而不是加注释解释"这个函数做了很多事"。

### 7.6 实现标准原则（重要 - AI 行为守则）

**做什么都是要做真正的实现，而不是为了减小工程量而投机取巧。任何近似方法必须经过用户审查和同意。**

背景：在实现 IBL (Step 3-4) 时，AI 自作主张用 skybox mip 近似替代了真正的 irradiance 和 prefiltered 贴图，没有告知用户，违反了"真正实现"的要求。

触发规则的条件：当 AI 在实现过程中意识到"完整实现太复杂/需要额外工具/需要额外基础设施"，产生"先用个简单方案顶着"的念头时，必须 STOP，向用户汇报完整方案和简化方案的差异，让用户决策。

这条规则适用于：
- 不把 `textureLod(skybox, N, high_mip)` 当 irradiance map 用
- 不把 `textureLod(skybox, R, roughness * mip)` 当 prefiltered 用
- 不用函数近似公式替代预计算纹理（如 BRDF LUT）
- 任何"先这样，以后再补"的念头都必须问用户
