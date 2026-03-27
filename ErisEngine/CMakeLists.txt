cmake_minimum_required(VERSION 3.15)
project(ErisEngine)

set(CMAKE_CXX_STANDARD 17)

# 包含路径
include_directories(include)
include_directories(third_party/vma) # 确保能找到 vk_mem_alloc.h

# 找到 Vulkan
find_package(Vulkan REQUIRED)

# 找到 GLFW (假设你已安装)
find_package(glfw3 REQUIRED)

# 源文件列表
set(SOURCES 
    src/main.cpp 
    src/ErisEngine.cpp 
    src/DeletionQueue.cpp
    src/VmaUsage.cpp
    src/VulkanInitializers.cpp
)

add_executable(ErisEngine ${SOURCES})

# 链接库
target_link_libraries(ErisEngine Vulkan::Vulkan glfw)