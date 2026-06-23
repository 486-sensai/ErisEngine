with open('D:/Users/Alice486/source/repos/ErisEngine/ErisEngine Tutorial/01-IBL.md', 'r', encoding='utf-8') as f:
    text = f.read()

start = text.find('## Step 6: 引擎加载 + 描述符扩展')
end = text.find('## Step 7: Shader 替换真正 IBL')

if start < 0 or end < 0:
    print("ERROR: markers not found")
    exit(1)

new_step6 = '''## Step 6: 引擎加载 + 描述符扩展

### 目标
加载 irradiance + prefiltered 贴图（prefiltered 带完整 9 级 GGX mip 链），并在描述符集中新增 binding。

### 操作 1/5：头文件加成员和声明

**文件**：`ErisEngine/include/ErisEngine.h`

找到 `AllocatedImage m_skyboxImage;`，在其下方添加：
```cpp
AllocatedImage m_irradianceMap;
AllocatedImage m_prefilteredMap;
```

找到 `loadBRDFLUT` 声明，在其下方添加：
```cpp
void loadPrefilteredMap();
```

### 操作 2/5：新建 loadPrefilteredMap 函数

**文件**：`ErisEngine/src/ErisEngine.cpp`

**位置**：在 `loadBRDFLUT` 函数之后（约第 540 行），插入新函数。

该函数加载 prefiltered 贴图的 9 级 mip x 6 面 = 54 个 .hdr 文件，不使用 blit 生成 mip，完整保留 GGX 预过滤数据。

```cpp
void ErisEngine::loadPrefilteredMap()
{
\tconst int numMips = 9;
\tconst int baseSize = 256;
\tconst char* faceSuffixes[6] = {"posx", "negx", "posy", "negy", "posz", "negz"};
\tstd::string basePath = "assets/ibl/prefiltered";

\t// 1. 计算 staging buffer 总大小
\tVkDeviceSize totalSize = 0;
\tint mipSizes[9];
\tfor (int mip = 0; mip < numMips; mip++) {
\t\tint size = std::max(baseSize >> mip, 1);
\t\tmipSizes[mip] = size;
\t\ttotalSize += size * size * 4 * sizeof(float) * 6;
\t}

\t// 2. 创建 staging buffer，加载所有 54 个文件
\tAllocatedBuffer staging = createBuffer(totalSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
\tvoid* mapped;
\tvmaMapMemory(m_allocator, staging.allocation, &mapped);
\tVkDeviceSize offset = 0;

\tfor (int mip = 0; mip < numMips; mip++) {
\t\tint sz = mipSizes[mip];
\t\tVkDeviceSize faceSize = sz * sz * 4 * sizeof(float);
\t\tfor (int face = 0; face < 6; face++) {
\t\t\tchar filename[256];
\t\t\tsprintf(filename, "%s/prefiltered_%s_%d_%dx%d.hdr",
\t\t\t\tbasePath.c_str(), faceSuffixes[face], mip, sz, sz);
\t\t\tint w, h, c;
\t\t\tfloat* pixels = stbi_loadf(filename, &w, &h, &c, STBI_rgb_alpha);
\t\t\tif (!pixels) {
\t\t\t\tthrow std::runtime_error(std::string("Failed: ") + filename);
\t\t\t}
\t\t\tmemcpy(static_cast<char*>(mapped) + offset, pixels, faceSize);
\t\t\tstbi_image_free(pixels);
\t\t\toffset += faceSize;
\t\t}
\t}
\tvmaUnmapMemory(m_allocator, staging.allocation);

\t// 3. 创建 GPU cubemap，mipLevels = numMips
\tAllocatedImage newImage;
\tVkImageCreateInfo imgInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
\timgInfo.imageType = VK_IMAGE_TYPE_2D;
\timgInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
\timgInfo.extent = { (uint32_t)baseSize, (uint32_t)baseSize, 1 };
\timgInfo.mipLevels = (uint32_t)numMips;
\timgInfo.arrayLayers = 6;
\timgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
\timgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
\timgInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
\timgInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

\tVmaAllocationCreateInfo allocInfo{ VMA_MEMORY_USAGE_GPU_ONLY };
\tvmaCreateImage(m_allocator, &imgInfo, &allocInfo, &newImage.image, &newImage.allocation, nullptr);

\t// 4. 逐 mip 逐面上传（不走 blit）
\toffset = 0;
\timmediateSubmit([&](VkCommandBuffer cmd) {
\t\tfor (uint32_t mip = 0; mip < (uint32_t)numMips; mip++) {
\t\t\tint sz = std::max(baseSize >> mip, 1);
\t\t\tVkDeviceSize faceSize = sz * sz * 4 * sizeof(float);
\t\t\tVkExtent3D mipExtent = { (uint32_t)sz, (uint32_t)sz, 1 };

\t\t\ttransitionImageLayout(cmd, newImage.image, VK_FORMAT_R32G32B32A32_SFLOAT,
\t\t\t\tVK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
\t\t\t\t6, 1, mip);

\t\t\tstd::vector<VkBufferImageCopy> regions(6);
\t\t\tfor (uint32_t f = 0; f < 6; f++) {
\t\t\t\tregions[f].bufferOffset = offset + faceSize * f;
\t\t\t\tregions[f].imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, mip, f, 1 };
\t\t\t\tregions[f].imageExtent = mipExtent;
\t\t\t}
\t\t\tvkCmdCopyBufferToImage(cmd, staging.buffer, newImage.image,
\t\t\t\tVK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 6, regions.data());

\t\t\ttransitionImageLayout(cmd, newImage.image, VK_FORMAT_R32G32B32A32_SFLOAT,
\t\t\t\tVK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
\t\t\t\t6, 1, mip);

\t\t\toffset += faceSize * 6;
\t\t}
\t\t});

\t// 5. ImageView
\tVkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
\tviewInfo.image = newImage.image;
\tviewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
\tviewInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
\tviewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, (uint32_t)numMips, 0, 6 };
\tvkCreateImageView(m_device, &viewInfo, nullptr, &newImage.imageView);

\tm_mainDeletionQueue.push_function([=]() {
\t\tvkDestroyImageView(m_device, newImage.imageView, nullptr);
\t\tvmaDestroyImage(m_allocator, newImage.image, newImage.allocation);
\t\t});
\tvmaDestroyBuffer(m_allocator, staging.buffer, staging.allocation);

\tm_prefilteredMap = newImage;
}
```

### 操作 3/5：在 Eris_init() 中加载两个贴图

**文件**：`ErisEngine/src/ErisEngine.cpp`

找到 `updateSkyboxDescriptor();` 一行，在其下方添加：

```cpp
// 加载 irradiance map
std::vector<std::string> irrFaces = {
\t"assets/ibl/irradiance_posx.hdr",
\t"assets/ibl/irradiance_negx.hdr",
\t"assets/ibl/irradiance_posy.hdr",
\t"assets/ibl/irradiance_negy.hdr",
\t"assets/ibl/irradiance_posz.hdr",
\t"assets/ibl/irradiance_negz.hdr",
};
m_irradianceMap = loadHDRCubemap(irrFaces);

// 加载 prefiltered map（完整 9 级 mip 链）
loadPrefilteredMap();
```

### 操作 4/5：描述符集扩展（Lumen + Forward）

**4a**：`initDescriptors()` 中为 `m_lumenDescriptorLayout` 新增 binding 5 和 6：

```cpp
// 在 brdfBind push_back 之后添加：
VkDescriptorSetLayoutBinding irrBind{};
irrBind.binding = 5;
irrBind.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
irrBind.descriptorCount = 1;
irrBind.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
lumenBinds.push_back(irrBind);

VkDescriptorSetLayoutBinding preBind{};
preBind.binding = 6;
preBind.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
preBind.descriptorCount = 1;
preBind.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
lumenBinds.push_back(preBind);
```

同样为 `m_iblDescriptorSetLayout`（Forward IBL Set 2）新增 binding 2 和 3：

```cpp
// 在 brdfBind push_back 之后添加：
VkDescriptorSetLayoutBinding fIrrBind{};
fIrrBind.binding = 2;
fIrrBind.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
fIrrBind.descriptorCount = 1;
fIrrBind.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
iblBinds.push_back(fIrrBind);

VkDescriptorSetLayoutBinding fPreBind{};
fPreBind.binding = 3;
fPreBind.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
fPreBind.descriptorCount = 1;
fPreBind.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
iblBinds.push_back(fPreBind);
```

**4b**：在 `updateLumenDescriptorSet()` 中新增两个写入（`writes` 数组从 5 改为 7，添加 binding 5/6）：

```cpp
VkDescriptorImageInfo irrInfo{};
irrInfo.sampler = m_skyboxSampler;
irrInfo.imageView = m_irradianceMap.imageView;
irrInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

VkDescriptorImageInfo preInfo{};
preInfo.sampler = m_skyboxSampler;
preInfo.imageView = m_prefilteredMap.imageView;
preInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

// 改为 std::array<VkWriteDescriptorSet, 7> writes{};
// 在 writes[4] (BRDF LUT) 之后添加：
writes[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
writes[5].dstSet = m_lumenDescriptorSet;
writes[5].dstBinding = 5;
writes[5].descriptorCount = 1;
writes[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
writes[5].pImageInfo = &irrInfo;

writes[6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
writes[6].dstSet = m_lumenDescriptorSet;
writes[6].dstBinding = 6;
writes[6].descriptorCount = 1;
writes[6].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
writes[6].pImageInfo = &preInfo;
```

同样在 `updateForwardIBLDescriptorSet()` 中新增 binding 2/3 写入（`writes` 数组从 2 改为 4）。

---

## Step 7: Shader 替换真正 IBL'''

if start >= 0 and end >= 0:
    text = text[:start] + new_step6 + text[end:]
    with open('D:/Users/Alice486/source/repos/ErisEngine/ErisEngine Tutorial/01-IBL.md', 'w', encoding='utf-8') as f:
        f.write(text)
    print("OK: Step 6 updated")
else:
    print(f"ERROR: markers at {start}, {end}")
