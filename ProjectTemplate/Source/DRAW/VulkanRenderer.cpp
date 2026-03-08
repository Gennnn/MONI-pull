#include "DrawComponents.h"
#include "../CCL.h"
#include "./Utility/TextureUtils.h"
#include "./Utility/CacheUtil.h"
// component dependencies
#include "./Utility/FileIntoString.h"
#include <filesystem>
#include "shaderc/shaderc.h" // needed for compiling shaders at runtime
#ifdef _WIN32 // must use MT platform DLL libraries on windows
#pragma comment(lib, "shaderc_combined.lib") 
#endif
#include "../UTIL/Utilities.h"

namespace DRAW
{
	//*** HELPER METHODS ***//

	VkViewport CreateViewportFromWindowDimensions(unsigned int windowWidth, unsigned int windowHeight)
	{
		VkViewport retval = {};
		retval.x = 0;
		retval.y = 0;
		retval.width = static_cast<float>(windowWidth);
		retval.height = static_cast<float>(windowHeight);
		retval.minDepth = 0;
		retval.maxDepth = 1;
		return retval;
	}

	VkRect2D CreateScissorFromWindowDimensions(unsigned int windowWidth, unsigned int windowHeight)
	{
		VkRect2D retval = {};
		retval.offset.x = 0;
		retval.offset.y = 0;
		retval.extent.width = windowWidth;
		retval.extent.height = windowHeight;
		return retval;
	}

	unsigned int CreateModelTextureFromImage(entt::registry& registry, entt::entity entity, tinygltf::Image& image, bool srgb);

	void InitializeTextures(entt::registry& registry, entt::entity entity) {
		auto& vulkanRenderer = registry.get<VulkanRenderer>(entity);
		if (!vulkanRenderer.textures.empty()) return;

		vulkanRenderer.textures.reserve(256);

		tinygltf::Image white{};
		white.width = 1;
		white.height = 1;
		white.component = 4;
		white.bits = 8;
		white.image = { 255,255,255,255 };
		CreateModelTextureFromImage(registry, entity, white, true);

		tinygltf::Image black = white;
		black.image = { 0,0,0,255 };

		tinygltf::Image gray = white;
		gray.image = { 128,128,128,255 };

		tinygltf::Image flatNormal = white;
		flatNormal.image = { 128,128,255,255 };

		CreateModelTextureFromImage(registry, entity, black, false);
		CreateModelTextureFromImage(registry, entity, gray, false);
		CreateModelTextureFromImage(registry, entity, flatNormal, false);
	}

	static std::string ExtractMtlTexturePath(const char* mtlValue)
	{
		if (!mtlValue || !*mtlValue) return {};

		std::string s = mtlValue;
		if (s.find(' ') == std::string::npos && s.find('\t') == std::string::npos)
			return s;

		std::istringstream iss(s);
		std::string tok, last;
		while (iss >> tok) last = tok;

		if (!last.empty() && last.front() == '"' && last.back() == '"')
			last = last.substr(1, last.size() - 2);

		return last;
	}

	static std::string ResolveTexturePath(const std::string& textureRoot, const char* materialPath)
	{

		if (!materialPath || !*materialPath) return "";

		std::filesystem::path filePath = std::filesystem::path(materialPath);

		if (std::filesystem::exists(filePath)) return filePath.generic_string();

		std::filesystem::path rooted = std::filesystem::path(textureRoot) / filePath;
		if (std::filesystem::exists(rooted)) return rooted.generic_string();

		std::filesystem::path justName = std::filesystem::path(textureRoot) / filePath.filename();
		if (std::filesystem::exists(justName)) return justName.generic_string();

		return rooted.generic_string(); 
	}

	unsigned int CreateModelTextureFromFile(entt::registry& registry, entt::entity entity, const std::string& path, bool srgb);

	static void BuildLevelTextureTable(entt::registry& registry, entt::entity entity, const std::string& textureRoot)
	{
		auto* cpuLevel = registry.try_get<DRAW::CPULevel>(entity);
		if (!cpuLevel) return;

		auto& level = cpuLevel->levelData;

		InitializeTextures(registry, entity);

		if (level.levelTextures.size() != level.levelMaterials.size())
			level.levelTextures.resize(level.levelMaterials.size());

		for (size_t i = 0; i < level.levelMaterials.size(); ++i)
		{
			const H2B::MATERIAL& m = level.levelMaterials[i];
			auto& out = level.levelTextures[i];

			out.albedoIndex = 0; 
			out.metalIndex = 1; 
			out.roughnessIndex = 2; 
			out.normalIndex = 3; 

			if (m.map_Kd && *m.map_Kd) {
				std::string path = ResolveTexturePath(textureRoot, m.map_Kd);
				out.albedoIndex = CreateModelTextureFromFile(registry, entity, path, true);
			}

			if (m.bump && *m.bump) {
				std::string bumpFile = ExtractMtlTexturePath(m.bump);
				std::string path = ResolveTexturePath(textureRoot, bumpFile.c_str());
				out.normalIndex = CreateModelTextureFromFile(registry, entity, path, false);
			}

			if (m.map_Ks && *m.map_Ks) {
				std::string path = ResolveTexturePath(textureRoot, m.map_Ks);
				out.roughnessIndex = CreateModelTextureFromFile(registry, entity, path, false);
			}

			if (m.map_Ka && *m.map_Ka) {
				std::string path = ResolveTexturePath(textureRoot, m.map_Ka);
				out.metalIndex = CreateModelTextureFromFile(registry, entity, path, false);
			}
		}
	}


	void LoadUITextures(entt::registry& registry, entt::entity entity) {

		auto& vulkanUIRenderer = registry.get<VulkanUIRenderer>(entity);

		for (const auto& file : std::filesystem::directory_iterator("../Textures/ui")) {

			std::string extension = file.path().extension().string();
			if (extension != ".png" && extension != ".jpg" && extension != ".jpeg") continue;

			std::string filePath = file.path().string();
			std::string key;
			MakeTextureKey(filePath, key);

			int textureId = CreateUITextureFromFile(registry, entity, file.path().generic_string());

			vulkanUIRenderer.textureCache[key] = textureId;
		}
	}

	void LoadFonts(entt::registry& registry, entt::entity entity, const std::vector<int>& sizes) {
		auto& vulkanUIRenderer = registry.get<VulkanUIRenderer>(entity);

		for (const auto& file : std::filesystem::directory_iterator("../Fonts")) {

			std::string extension = file.path().extension().string();
			if (extension != ".ttf") continue;

			std::string filePath = file.path().string();
			std::string key;

			for (int size : sizes) {
				int id = GetOrCreateFont(registry, entity, filePath, size);
				if (id >= 0) {
					MakeFontKey(filePath, size, key);
				}
			}

		}
	}

	void InitializeDescriptors(entt::registry& registry, entt::entity entity)
	{
		auto& vulkanRenderer = registry.get<VulkanRenderer>(entity);

		unsigned int frameCount;
		vulkanRenderer.vlkSurface.GetSwapchainImageCount(frameCount);
		vulkanRenderer.descriptorSets.resize(frameCount);

#pragma region Descriptor Layout
		VkDescriptorSetLayoutBinding layoutBinding[2] = {};
		layoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		layoutBinding[0].descriptorCount = 1;
		layoutBinding[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		layoutBinding[0].binding = 0;
		layoutBinding[0].pImmutableSamplers = nullptr;
		
		layoutBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		layoutBinding[1].descriptorCount = 1;
		layoutBinding[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		layoutBinding[1].binding = 1;
		layoutBinding[1].pImmutableSamplers = nullptr;

		VkDescriptorSetLayoutCreateInfo setCreateInfo = {};
		setCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		setCreateInfo.bindingCount = 2;
		setCreateInfo.pBindings = layoutBinding;
		setCreateInfo.flags = 0;
		setCreateInfo.pNext = nullptr;
		vkCreateDescriptorSetLayout(vulkanRenderer.device, &setCreateInfo, nullptr, &vulkanRenderer.descriptorLayout);
#pragma endregion

#pragma region Descriptor Pool
		VkDescriptorPoolCreateInfo descriptorpool_create_info = {};
		descriptorpool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		VkDescriptorPoolSize descriptorpool_size[2] = {
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameCount },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, frameCount }
		};
		descriptorpool_create_info.poolSizeCount = 2;
		descriptorpool_create_info.pPoolSizes = descriptorpool_size;
		descriptorpool_create_info.maxSets = frameCount;
		descriptorpool_create_info.flags = 0;
		descriptorpool_create_info.pNext = nullptr;
		vkCreateDescriptorPool(vulkanRenderer.device, &descriptorpool_create_info, nullptr, &vulkanRenderer.descriptorPool);
#pragma endregion

#pragma region Allocate Descriptor Sets
		VkDescriptorSetAllocateInfo allocateInfo = {};
		allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocateInfo.descriptorSetCount = 1;
		allocateInfo.descriptorPool = vulkanRenderer.descriptorPool;
		allocateInfo.pSetLayouts = &vulkanRenderer.descriptorLayout;
		allocateInfo.pNext = nullptr;
		for (int i = 0; i < frameCount; i++)
		{
			vkAllocateDescriptorSets(vulkanRenderer.device, &allocateInfo, &vulkanRenderer.descriptorSets[i]);
		}
#pragma endregion

		// Add the 2 buffers, this will create the initial buffers so we can finish building our descriptor set
		auto& storageBuffer = registry.emplace<VulkanGPUInstanceBuffer>(entity,
			VulkanGPUInstanceBuffer{16}); // Start with a reasonable size of elements. The Buffer will grow if it needs to later
		auto& uniformBuffer = registry.emplace<VulkanUniformBuffer>(entity);

		 
		for (int i = 0; i < frameCount; i++)
		{
			
			VkDescriptorBufferInfo uniformBufferInfo = { uniformBuffer.buffer[i], 0, VK_WHOLE_SIZE};
			VkWriteDescriptorSet uniformWrite = {};
			uniformWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			uniformWrite.dstSet = vulkanRenderer.descriptorSets[i];
			uniformWrite.dstBinding = 0; // 0 For the uniform buffer
			uniformWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			uniformWrite.descriptorCount = 1;
			uniformWrite.pBufferInfo = &uniformBufferInfo;

			VkDescriptorBufferInfo storageBufferInfo = { storageBuffer.buffer[i], 0, VK_WHOLE_SIZE };
			VkWriteDescriptorSet storageWrite = {};
			storageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			storageWrite.dstSet = vulkanRenderer.descriptorSets[i];
			storageWrite.dstBinding = 1; // 1 For the storage buffer
			storageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			storageWrite.descriptorCount = 1;
			storageWrite.pBufferInfo = &storageBufferInfo;

			VkWriteDescriptorSet descriptorWrites[] = { uniformWrite, storageWrite };
			vkUpdateDescriptorSets(vulkanRenderer.device, 2, descriptorWrites, 0, nullptr);
		}

	}

	static void WriteMaterialSet(VkDevice device, VkDescriptorSet descriptorSet, VkImageView albedo, VkImageView normal, VkImageView rough, VkImageView metal, VkSampler sampler) {
		VkDescriptorImageInfo images[4]{};
		images[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		images[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		images[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		images[3].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		images[0].sampler = VK_NULL_HANDLE;
		images[1].sampler = VK_NULL_HANDLE;
		images[2].sampler = VK_NULL_HANDLE;
		images[3].sampler = VK_NULL_HANDLE;

		images[0].imageView = albedo;
		images[1].imageView = normal;
		images[2].imageView = rough;
		images[3].imageView = metal;

		VkDescriptorImageInfo samplerImageInfo{};
		samplerImageInfo.sampler = sampler;

		VkWriteDescriptorSet write_descriptor_set[5]{};
		for (int i = 0; i < 4; i++) {
			write_descriptor_set[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write_descriptor_set[i].dstSet = descriptorSet;
			write_descriptor_set[i].dstBinding = i;
			write_descriptor_set[i].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			write_descriptor_set[i].descriptorCount = 1;
			write_descriptor_set[i].pImageInfo = &images[i];
		}

		write_descriptor_set[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write_descriptor_set[4].dstSet = descriptorSet;
		write_descriptor_set[4].dstBinding = 4;
		write_descriptor_set[4].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
		write_descriptor_set[4].descriptorCount = 1;
		write_descriptor_set[4].pImageInfo = &samplerImageInfo;

		vkUpdateDescriptorSets(device, 5, write_descriptor_set, 0, nullptr);
	}

	VkImageView GetTextureImageView(int materialIndex, VulkanRenderer& vulkanRenderer) {
		if (materialIndex < vulkanRenderer.textures.size()) return vulkanRenderer.textures[materialIndex].imageView;
		return vulkanRenderer.textures[0].imageView;
	}

	void BuildMaterialSets(entt::registry& registry, entt::entity entity) {
		auto& vulkanRenderer = registry.get<DRAW::VulkanRenderer>(entity);
		auto* cpuLevel = registry.try_get<DRAW::CPULevel>(entity);

		if (cpuLevel == nullptr) return;

		auto& level = cpuLevel->levelData;

		if (vulkanRenderer.materialSets.size() != level.levelMaterials.size()) return;

		InitializeTextures(registry, entity);

		if (level.levelTextures.size() != level.levelMaterials.size()) {
			level.levelTextures.resize(level.levelMaterials.size());
		}

		for (int matIndex = 0; matIndex < level.levelMaterials.size(); matIndex++) {
			auto texture = level.levelTextures[matIndex];

			VkImageView albedo = GetTextureImageView(texture.albedoIndex, vulkanRenderer);
			VkImageView normal = GetTextureImageView(texture.normalIndex, vulkanRenderer);
			VkImageView rough = GetTextureImageView(texture.roughnessIndex, vulkanRenderer);
			VkImageView metal = GetTextureImageView(texture.metalIndex, vulkanRenderer);
			

			WriteMaterialSet(vulkanRenderer.device, vulkanRenderer.materialSets[matIndex], albedo, normal, rough, metal, vulkanRenderer.materialSampler);
		}
	}
	void InitializeMaterialDescriptors(entt::registry& registry, entt::entity entity) {
		auto& vulkanRenderer = registry.get<DRAW::VulkanRenderer>(entity);
		
		if (vulkanRenderer.materialSetLayout != nullptr) return;

		VkDescriptorSetLayoutBinding descriptor_set_layout_binding[5]{};

		for (int i = 0; i < 4; i++) {
			descriptor_set_layout_binding[i].binding = i;
			descriptor_set_layout_binding[i].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			descriptor_set_layout_binding[i].descriptorCount = 1;
			descriptor_set_layout_binding[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		}

		descriptor_set_layout_binding[4].binding = 4;
		descriptor_set_layout_binding[4].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
		descriptor_set_layout_binding[4].descriptorCount = 1;
		descriptor_set_layout_binding[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutCreateInfo layout_create_info{};
		layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layout_create_info.bindingCount = 5;
		layout_create_info.pBindings = descriptor_set_layout_binding;

		vkCreateDescriptorSetLayout(vulkanRenderer.device, &layout_create_info, nullptr, &vulkanRenderer.materialSetLayout);

		CreateSampler(vulkanRenderer.vlkSurface, vulkanRenderer.materialSampler, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_FILTER_LINEAR);
	}

	void AllocateMaterialDescriptorSet(entt::registry& registry, entt::entity entity, VulkanRenderer& vulkanRenderer) {
		auto* cpuLevel = registry.try_get<DRAW::CPULevel>(entity);
		if (cpuLevel == nullptr) return;

		unsigned int materialCount = cpuLevel->levelData.levelMaterials.size();
		if (materialCount == 0) return;

		if (vulkanRenderer.materialPool != nullptr) {
			vkDestroyDescriptorPool(vulkanRenderer.device, vulkanRenderer.materialPool, nullptr);
			vulkanRenderer.materialPool = nullptr;
			vulkanRenderer.materialSets.clear();
		}

		VkDescriptorPoolSize descriptor_pool_sizes[2]{};
		descriptor_pool_sizes[0].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		descriptor_pool_sizes[0].descriptorCount = materialCount * 4;
		descriptor_pool_sizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLER;
		descriptor_pool_sizes[1].descriptorCount = materialCount;

		VkDescriptorPoolCreateInfo descriptor_pool_create_info{};
		descriptor_pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptor_pool_create_info.poolSizeCount = 2;
		descriptor_pool_create_info.pPoolSizes = descriptor_pool_sizes;
		descriptor_pool_create_info.maxSets = materialCount;

		vkCreateDescriptorPool(vulkanRenderer.device, &descriptor_pool_create_info, nullptr, &vulkanRenderer.materialPool);

		vulkanRenderer.materialSets.resize(materialCount);

		std::vector<VkDescriptorSetLayout> descriptorSetLayouts(materialCount, vulkanRenderer.materialSetLayout);
		VkDescriptorSetAllocateInfo descriptor_set_allocate_info{};
		descriptor_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptor_set_allocate_info.descriptorPool = vulkanRenderer.materialPool;
		descriptor_set_allocate_info.descriptorSetCount = materialCount;
		descriptor_set_allocate_info.pSetLayouts = descriptorSetLayouts.data();

		vkAllocateDescriptorSets(vulkanRenderer.device, &descriptor_set_allocate_info, vulkanRenderer.materialSets.data());
	}

	static void LoadMaterials(entt::registry& registry, entt::entity entity) {
		auto& vulkanRenderer = registry.get<VulkanRenderer>(entity);
		auto* cpuLevel = registry.try_get<CPULevel>(entity);

		if (cpuLevel == nullptr) return;

		InitializeMaterialDescriptors(registry, entity);

		BuildLevelTextureTable(registry, entity, "../Textures/models");

		int materialCount = cpuLevel->levelData.levelMaterials.size();
		if (materialCount == 0) return;

		if (vulkanRenderer.materialSets.size() != materialCount) {
			AllocateMaterialDescriptorSet(registry, entity, vulkanRenderer);
		}

		BuildMaterialSets(registry, entity);
	}

	VkDescriptorSet AllocateUIDescriptorSet(const VulkanRenderer& vulkanRenderer, VulkanUIRenderer& vulkanUIRenderer);
	void WriteUIDescriptorSets(const VulkanRenderer& vulkanRenderer, VkDescriptorSet descriptorSet, VkImageView imageView, VkSampler sampler);

	void InitializeUIDescriptors(entt::registry& registry, entt::entity entity)
	{
		auto& vulkanRenderer = registry.get<VulkanRenderer>(entity);
		auto& vulkanUIRenderer = registry.get<VulkanUIRenderer>(entity);

#pragma region Descriptor Layout
		VkDescriptorSetLayoutBinding layoutBinding[2] = {};
		layoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		layoutBinding[0].descriptorCount = 1;
		layoutBinding[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		layoutBinding[0].binding = 0;
		layoutBinding[0].pImmutableSamplers = nullptr;

		layoutBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
		layoutBinding[1].descriptorCount = 1;
		layoutBinding[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		layoutBinding[1].binding = 1;
		layoutBinding[1].pImmutableSamplers = nullptr;

		VkDescriptorSetLayoutCreateInfo setCreateInfo = {};
		setCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		setCreateInfo.bindingCount = 2;
		setCreateInfo.pBindings = layoutBinding;
		setCreateInfo.flags = 0;
		setCreateInfo.pNext = nullptr;
		vkCreateDescriptorSetLayout(vulkanRenderer.device, &setCreateInfo, nullptr, &vulkanUIRenderer.descriptorSetLayout);
#pragma endregion

		unsigned int maxTextures = 128;
		
#pragma region Descriptor Pool
		VkDescriptorPoolCreateInfo descriptorpool_create_info = {};
		descriptorpool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		VkDescriptorPoolSize descriptorpool_size[2] = {
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, maxTextures },
			{ VK_DESCRIPTOR_TYPE_SAMPLER, maxTextures }
		};
		descriptorpool_create_info.poolSizeCount = 2;
		descriptorpool_create_info.pPoolSizes = descriptorpool_size;
		descriptorpool_create_info.maxSets = maxTextures;
		descriptorpool_create_info.flags = 0;
		descriptorpool_create_info.pNext = nullptr;
		vkCreateDescriptorPool(vulkanRenderer.device, &descriptorpool_create_info, nullptr, &vulkanUIRenderer.descriptorPool);
#pragma endregion

		vulkanUIRenderer.textures.clear();
		vulkanUIRenderer.textures.reserve(maxTextures);

		UITexture white{};

		tinygltf::Image img{};
		img.width = 1;
		img.height = 1;
		img.component = 4;
		img.bits = 8;
		img.image = { 255,255,255,255 };

		UploadTextureToGPU(vulkanRenderer.vlkSurface, img, white.memory, white.image, white.imageView, true);
		CreateSampler(vulkanRenderer.vlkSurface, white.sampler, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FILTER_NEAREST);

		white.set = AllocateUIDescriptorSet(vulkanRenderer, vulkanUIRenderer);
		WriteUIDescriptorSets(vulkanRenderer, white.set, white.imageView, white.sampler);

		white.width;
		white.height;

		vulkanUIRenderer.textures.push_back(white);

		LoadUITextures(registry, entity);
		LoadFonts(registry, entity, { 12, 18, 30, 36, 48 });
	}

	
	

	VkDescriptorSet AllocateUIDescriptorSet(const VulkanRenderer& vulkanRenderer, VulkanUIRenderer& vulkanUIRenderer) {
		VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

		VkDescriptorSetAllocateInfo descriptor_set_allocate_info{};
		descriptor_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptor_set_allocate_info.descriptorPool = vulkanUIRenderer.descriptorPool;
		descriptor_set_allocate_info.descriptorSetCount = 1;
		descriptor_set_allocate_info.pSetLayouts = &vulkanUIRenderer.descriptorSetLayout;

		vkAllocateDescriptorSets(vulkanRenderer.device, &descriptor_set_allocate_info, &descriptorSet);

		return descriptorSet;
	}

	void WriteUIDescriptorSets(const VulkanRenderer& vulkanRenderer, VkDescriptorSet descriptorSet, VkImageView imageView, VkSampler sampler) {
		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageView = imageView;
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkDescriptorImageInfo samplerInfo{};
		samplerInfo.sampler = sampler;

		VkWriteDescriptorSet writes[2]{};

		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet = descriptorSet;
		writes[0].dstBinding = 0;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		writes[0].descriptorCount = 1;
		writes[0].pImageInfo = &imageInfo;

		writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[1].dstSet = descriptorSet;
		writes[1].dstBinding = 1;
		writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
		writes[1].descriptorCount = 1;
		writes[1].pImageInfo = &samplerInfo;

		vkUpdateDescriptorSets(vulkanRenderer.device, 2, writes, 0, nullptr);
	}

	unsigned int CreateModelTextureFromFile(entt::registry& registry, entt::entity entity, const std::string& path, bool srgb) {
		auto& vulkanRenderer = registry.get<VulkanRenderer>(entity);

		InitializeTextures(registry, entity);

		std::string key;
		MakeTextureKey(path, key);

		auto textureCacheIter = vulkanRenderer.textureCache.find(key);
		if (textureCacheIter != vulkanRenderer.textureCache.end()) {
			return textureCacheIter->second;
		}

		ModelTexture texture{};

		const bool linearColorSpace = !srgb;

		UploadTextureToGPU(vulkanRenderer.vlkSurface, path, texture.memory, texture.image, texture.imageView, linearColorSpace);

		vulkanRenderer.textures.push_back(texture);
		unsigned int id = static_cast<unsigned int>(vulkanRenderer.textures.size() - 1);

		vulkanRenderer.textureCache[key] = id;
		return id;
	}

	unsigned int CreateModelTextureFromImage(entt::registry& registry, entt::entity entity, tinygltf::Image& image, bool srgb) {
		auto& vulkanRenderer = registry.get<VulkanRenderer>(entity);

		ModelTexture texture{};

		const bool linearColorSpace = !srgb;

		UploadTextureToGPU(vulkanRenderer.vlkSurface, image, texture.memory, texture.image, texture.imageView, linearColorSpace);

		vulkanRenderer.textures.push_back(texture);
		return static_cast<unsigned int>(vulkanRenderer.textures.size() - 1);
	}

	unsigned int CreateUITextureFromFile(entt::registry& registry, entt::entity entity, const std::string& path, bool srgb) {
		auto& vulkanRenderer = registry.get<VulkanRenderer>(entity);
		auto& vulkanUIRenderer = registry.get<VulkanUIRenderer>(entity);

		UITexture texture{};

		const bool linearColorSpace = !srgb;

		UploadTextureToGPU(vulkanRenderer.vlkSurface, path, texture.memory, texture.image, texture.imageView, linearColorSpace);

		CreateSampler(vulkanRenderer.vlkSurface, texture.sampler, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FILTER_NEAREST);

		texture.set = AllocateUIDescriptorSet(vulkanRenderer, vulkanUIRenderer);
		WriteUIDescriptorSets(vulkanRenderer, texture.set, texture.imageView, texture.sampler);

		vulkanUIRenderer.textures.push_back(texture);
		return static_cast<unsigned int>(vulkanUIRenderer.textures.size() - 1);
	}

	unsigned int CreateUITextureFromImage(entt::registry& registry, entt::entity entity, tinygltf::Image& image, bool srgb) {
		auto& vulkanRenderer = registry.get<VulkanRenderer>(entity);
		auto& vulkanUIRenderer = registry.get<VulkanUIRenderer>(entity);

		UITexture texture{};

		const bool linearColorSpace = !srgb;

		UploadTextureToGPU(vulkanRenderer.vlkSurface, image, texture.memory, texture.image, texture.imageView, linearColorSpace);

		CreateSampler(vulkanRenderer.vlkSurface, texture.sampler, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FILTER_NEAREST);

		texture.set = AllocateUIDescriptorSet(vulkanRenderer, vulkanUIRenderer);
		WriteUIDescriptorSets(vulkanRenderer, texture.set, texture.imageView, texture.sampler);

		texture.width = image.width;
		texture.height = image.height;

		vulkanUIRenderer.textures.push_back(texture);
		return static_cast<unsigned int>(vulkanUIRenderer.textures.size() - 1);
	}

	void DestroyUITexture(const VulkanRenderer& renderer, UITexture& texture) {
		if (texture.sampler) vkDestroySampler(renderer.device, texture.sampler, nullptr);
		if (texture.imageView) vkDestroyImageView(renderer.device, texture.imageView, nullptr);
		if (texture.image) vkDestroyImage(renderer.device, texture.image, nullptr);
		if (texture.memory) vkFreeMemory(renderer.device, texture.memory, nullptr);
		texture = {};
	}

	void InitializeGraphicsPipeline(entt::registry& registry, entt::entity entity)
	{
		auto& vulkanRenderer = registry.get<VulkanRenderer>(entity);
		GW::SYSTEM::GWindow win = registry.get<GW::SYSTEM::GWindow>(entity);

		// Create Pipeline & Layout (Thanks Tiny!)
		VkPipelineShaderStageCreateInfo stage_create_info[2] = {};
		// Create Stage Info for Vertex Shader
		stage_create_info[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stage_create_info[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		stage_create_info[0].module = vulkanRenderer.vertexShader;
		stage_create_info[0].pName = "main";

		// Create Stage Info for Fragment Shader
		stage_create_info[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stage_create_info[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		stage_create_info[1].module = vulkanRenderer.fragmentShader;
		stage_create_info[1].pName = "main";

		VkPipelineInputAssemblyStateCreateInfo assembly_create_info = {};
		assembly_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		assembly_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		assembly_create_info.primitiveRestartEnable = false;

		VkVertexInputBindingDescription vertex_binding_description = {};
		vertex_binding_description.binding = 0;
		vertex_binding_description.stride = sizeof(H2B::VERTEX);
		vertex_binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		VkVertexInputAttributeDescription vertex_attribute_description[3];
		vertex_attribute_description[0].binding = 0;
		vertex_attribute_description[0].location = 0;
		vertex_attribute_description[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		vertex_attribute_description[0].offset = offsetof(H2B::VERTEX, pos);

		vertex_attribute_description[1].binding = 0;
		vertex_attribute_description[1].location = 1;
		vertex_attribute_description[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		vertex_attribute_description[1].offset = offsetof(H2B::VERTEX, uvw);

		vertex_attribute_description[2].binding = 0;
		vertex_attribute_description[2].location = 2;
		vertex_attribute_description[2].format = VK_FORMAT_R32G32B32_SFLOAT;
		vertex_attribute_description[2].offset = offsetof(H2B::VERTEX, nrm);

		VkPipelineVertexInputStateCreateInfo input_vertex_info = {};
		input_vertex_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		input_vertex_info.vertexBindingDescriptionCount = 1;
		input_vertex_info.pVertexBindingDescriptions = &vertex_binding_description;
		input_vertex_info.vertexAttributeDescriptionCount = 3;
		input_vertex_info.pVertexAttributeDescriptions = vertex_attribute_description;

		unsigned int windowWidth, windowHeight;
		win.GetClientWidth(windowWidth);
		win.GetClientHeight(windowHeight);
		VkViewport viewport = CreateViewportFromWindowDimensions(windowWidth, windowHeight);

		VkRect2D scissor = CreateScissorFromWindowDimensions(windowWidth, windowHeight);

		VkPipelineViewportStateCreateInfo viewport_create_info = {};
		viewport_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewport_create_info.viewportCount = 1;
		viewport_create_info.pViewports = &viewport;
		viewport_create_info.scissorCount = 1;
		viewport_create_info.pScissors = &scissor;

		VkPipelineRasterizationStateCreateInfo rasterization_create_info = {};
		rasterization_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterization_create_info.rasterizerDiscardEnable = VK_FALSE;
		rasterization_create_info.polygonMode = VK_POLYGON_MODE_FILL;
		rasterization_create_info.lineWidth = 1.0f;
		rasterization_create_info.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterization_create_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
		rasterization_create_info.depthClampEnable = VK_FALSE;
		rasterization_create_info.depthBiasEnable = VK_FALSE;
		rasterization_create_info.depthBiasClamp = 0.0f;
		rasterization_create_info.depthBiasConstantFactor = 0.0f;
		rasterization_create_info.depthBiasSlopeFactor = 0.0f;

		VkPipelineMultisampleStateCreateInfo multisample_create_info = {};
		multisample_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisample_create_info.sampleShadingEnable = VK_FALSE;
		multisample_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisample_create_info.minSampleShading = 1.0f;
		multisample_create_info.pSampleMask = VK_NULL_HANDLE;
		multisample_create_info.alphaToCoverageEnable = VK_FALSE;
		multisample_create_info.alphaToOneEnable = VK_FALSE;

		VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info = {};
		depth_stencil_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depth_stencil_create_info.depthTestEnable = VK_TRUE;
		depth_stencil_create_info.depthWriteEnable = VK_TRUE;
		depth_stencil_create_info.depthCompareOp = VK_COMPARE_OP_LESS;
		depth_stencil_create_info.depthBoundsTestEnable = VK_FALSE;
		depth_stencil_create_info.minDepthBounds = 0.0f;
		depth_stencil_create_info.maxDepthBounds = 1.0f;
		depth_stencil_create_info.stencilTestEnable = VK_FALSE;

		VkPipelineColorBlendAttachmentState color_blend_attachment_state = {};
		color_blend_attachment_state.colorWriteMask = 0xF;
		color_blend_attachment_state.blendEnable = VK_FALSE;
		color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
		color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
		color_blend_attachment_state.colorBlendOp = VK_BLEND_OP_ADD;
		color_blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		color_blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
		color_blend_attachment_state.alphaBlendOp = VK_BLEND_OP_ADD;

		VkPipelineColorBlendStateCreateInfo color_blend_create_info = {};
		color_blend_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		color_blend_create_info.logicOpEnable = VK_FALSE;
		color_blend_create_info.logicOp = VK_LOGIC_OP_COPY;
		color_blend_create_info.attachmentCount = 1;
		color_blend_create_info.pAttachments = &color_blend_attachment_state;
		color_blend_create_info.blendConstants[0] = 0.0f;
		color_blend_create_info.blendConstants[1] = 0.0f;
		color_blend_create_info.blendConstants[2] = 0.0f;
		color_blend_create_info.blendConstants[3] = 0.0f;

		// Dynamic State 
		VkDynamicState dynamic_states[2] =
		{
			// By setting these we do not need to re-create the pipeline on Resize
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};
		VkPipelineDynamicStateCreateInfo dynamic_create_info = {};
		dynamic_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamic_create_info.dynamicStateCount = 2;
		dynamic_create_info.pDynamicStates = dynamic_states;

		InitializeDescriptors(registry, entity);
		InitializeMaterialDescriptors(registry, entity);

		VkDescriptorSetLayout descriptor_set_layouts[2] = { vulkanRenderer.descriptorLayout, vulkanRenderer.materialSetLayout };

		VkPipelineLayoutCreateInfo pipeline_layout_create_info = {};
		pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipeline_layout_create_info.setLayoutCount = 2;
		pipeline_layout_create_info.pSetLayouts = descriptor_set_layouts;
		pipeline_layout_create_info.pushConstantRangeCount = 0;
		pipeline_layout_create_info.pPushConstantRanges = nullptr;

		vkCreatePipelineLayout(vulkanRenderer.device, &pipeline_layout_create_info, nullptr, &vulkanRenderer.pipelineLayout);

		// Pipeline State... (FINALLY) 
		VkGraphicsPipelineCreateInfo pipeline_create_info = {};
		pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipeline_create_info.stageCount = 2;
		pipeline_create_info.pStages = stage_create_info;
		pipeline_create_info.pInputAssemblyState = &assembly_create_info;
		pipeline_create_info.pVertexInputState = &input_vertex_info;
		pipeline_create_info.pViewportState = &viewport_create_info;
		pipeline_create_info.pRasterizationState = &rasterization_create_info;
		pipeline_create_info.pMultisampleState = &multisample_create_info;
		pipeline_create_info.pDepthStencilState = &depth_stencil_create_info;
		pipeline_create_info.pColorBlendState = &color_blend_create_info;
		pipeline_create_info.pDynamicState = &dynamic_create_info;
		pipeline_create_info.layout = vulkanRenderer.pipelineLayout;
		pipeline_create_info.renderPass = vulkanRenderer.renderPass;
		pipeline_create_info.subpass = 0;
		pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;

		vkCreateGraphicsPipelines(vulkanRenderer.device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &vulkanRenderer.pipeline);
	}

	void InitializeUIGraphicsPipeline(entt::registry& registry, entt::entity entity)
	{
		auto& vulkanRenderer = registry.get<VulkanRenderer>(entity);
		auto& vulkanUIRenderer = registry.get<VulkanUIRenderer>(entity);
		GW::SYSTEM::GWindow win = registry.get<GW::SYSTEM::GWindow>(entity);

		// Create Pipeline & Layout (Thanks Tiny!)
		VkPipelineShaderStageCreateInfo stage_create_info[2] = {};
		// Create Stage Info for Vertex Shader
		stage_create_info[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stage_create_info[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		stage_create_info[0].module = vulkanUIRenderer.vertexShader;
		stage_create_info[0].pName = "main";

		// Create Stage Info for Fragment Shader
		stage_create_info[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stage_create_info[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		stage_create_info[1].module = vulkanUIRenderer.fragmentShader;
		stage_create_info[1].pName = "main";

		VkPipelineInputAssemblyStateCreateInfo assembly_create_info = {};
		assembly_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		assembly_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		assembly_create_info.primitiveRestartEnable = false;

		VkVertexInputBindingDescription vertex_binding_description = {};
		vertex_binding_description.binding = 0;
		vertex_binding_description.stride = sizeof(UIVertex);
		vertex_binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		VkVertexInputAttributeDescription vertex_attribute_description[3];
		vertex_attribute_description[0].binding = 0;
		vertex_attribute_description[0].location = 0;
		vertex_attribute_description[0].format = VK_FORMAT_R32G32_SFLOAT;
		vertex_attribute_description[0].offset = offsetof(UIVertex, pos);

		vertex_attribute_description[1].binding = 0;
		vertex_attribute_description[1].location = 1;
		vertex_attribute_description[1].format = VK_FORMAT_R32G32_SFLOAT;
		vertex_attribute_description[1].offset = offsetof(UIVertex, uv);

		vertex_attribute_description[2].binding = 0;
		vertex_attribute_description[2].location = 2;
		vertex_attribute_description[2].format = VK_FORMAT_R8G8B8A8_UNORM;
		vertex_attribute_description[2].offset = offsetof(UIVertex, color);

		VkPipelineVertexInputStateCreateInfo input_vertex_info = {};
		input_vertex_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		input_vertex_info.vertexBindingDescriptionCount = 1;
		input_vertex_info.pVertexBindingDescriptions = &vertex_binding_description;
		input_vertex_info.vertexAttributeDescriptionCount = 3;
		input_vertex_info.pVertexAttributeDescriptions = vertex_attribute_description;

		unsigned int windowWidth, windowHeight;
		win.GetClientWidth(windowWidth);
		win.GetClientHeight(windowHeight);
		VkViewport viewport = CreateViewportFromWindowDimensions(windowWidth, windowHeight);

		VkRect2D scissor = CreateScissorFromWindowDimensions(windowWidth, windowHeight);

		VkPipelineViewportStateCreateInfo viewport_create_info = {};
		viewport_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewport_create_info.viewportCount = 1;
		viewport_create_info.pViewports = &viewport;
		viewport_create_info.scissorCount = 1;
		viewport_create_info.pScissors = &scissor;

		VkPipelineRasterizationStateCreateInfo rasterization_create_info = {};
		rasterization_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterization_create_info.rasterizerDiscardEnable = VK_FALSE;
		rasterization_create_info.polygonMode = VK_POLYGON_MODE_FILL;
		rasterization_create_info.lineWidth = 1.0f;
		rasterization_create_info.cullMode = VK_CULL_MODE_NONE;
		rasterization_create_info.frontFace = VK_FRONT_FACE_CLOCKWISE;

		VkPipelineMultisampleStateCreateInfo multisample_create_info = {};
		multisample_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisample_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info = {};
		depth_stencil_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depth_stencil_create_info.depthTestEnable = VK_FALSE;
		depth_stencil_create_info.depthWriteEnable = VK_FALSE;
		depth_stencil_create_info.stencilTestEnable = VK_FALSE;

		VkPipelineColorBlendAttachmentState color_blend_attachment_state = {};
		color_blend_attachment_state.colorWriteMask = 0xF;
		color_blend_attachment_state.blendEnable = VK_TRUE;
		color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		color_blend_attachment_state.colorBlendOp = VK_BLEND_OP_ADD;
		color_blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		color_blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		color_blend_attachment_state.alphaBlendOp = VK_BLEND_OP_ADD;

		VkPipelineColorBlendStateCreateInfo color_blend_create_info = {};
		color_blend_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		color_blend_create_info.attachmentCount = 1;
		color_blend_create_info.pAttachments = &color_blend_attachment_state;

		// Dynamic State 
		VkDynamicState dynamic_states[2] =
		{
			// By setting these we do not need to re-create the pipeline on Resize
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};
		VkPipelineDynamicStateCreateInfo dynamic_create_info = {};
		dynamic_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamic_create_info.dynamicStateCount = 2;
		dynamic_create_info.pDynamicStates = dynamic_states;

		InitializeUIDescriptors(registry, entity);

		VkPushConstantRange push_constant_range = {};
		push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		push_constant_range.offset = 0;
		push_constant_range.size = sizeof(UIPush);

		VkPipelineLayoutCreateInfo pipeline_layout_create_info = {};
		pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipeline_layout_create_info.setLayoutCount = 1;
		pipeline_layout_create_info.pSetLayouts = &vulkanUIRenderer.descriptorSetLayout;
		pipeline_layout_create_info.pushConstantRangeCount = 1;
		pipeline_layout_create_info.pPushConstantRanges = &push_constant_range;

		vkCreatePipelineLayout(vulkanRenderer.device, &pipeline_layout_create_info, nullptr, &vulkanUIRenderer.pipelineLayout);

		// Pipeline State... (FINALLY) 
		VkGraphicsPipelineCreateInfo pipeline_create_info = {};
		pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipeline_create_info.stageCount = 2;
		pipeline_create_info.pStages = stage_create_info;
		pipeline_create_info.pInputAssemblyState = &assembly_create_info;
		pipeline_create_info.pVertexInputState = &input_vertex_info;
		pipeline_create_info.pViewportState = &viewport_create_info;
		pipeline_create_info.pRasterizationState = &rasterization_create_info;
		pipeline_create_info.pMultisampleState = &multisample_create_info;
		pipeline_create_info.pDepthStencilState = &depth_stencil_create_info;
		pipeline_create_info.pColorBlendState = &color_blend_create_info;
		pipeline_create_info.pDynamicState = &dynamic_create_info;
		pipeline_create_info.layout = vulkanUIRenderer.pipelineLayout;
		pipeline_create_info.renderPass = vulkanRenderer.renderPass;
		pipeline_create_info.subpass = 0;
		pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;

		vkCreateGraphicsPipelines(vulkanRenderer.device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &vulkanUIRenderer.pipeline);
	}

	void InitializeSkyboxGraphicsPipeline(entt::registry& registry, entt::entity entity)
	{
		auto& vulkanRenderer = registry.get<VulkanRenderer>(entity);
		GW::SYSTEM::GWindow win = registry.get<GW::SYSTEM::GWindow>(entity);

		// Create Pipeline & Layout (Thanks Tiny!)
		VkPipelineShaderStageCreateInfo stage_create_info[2] = {};
		// Create Stage Info for Vertex Shader
		stage_create_info[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stage_create_info[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		stage_create_info[0].module = vulkanRenderer.skyboxVertexShader;
		stage_create_info[0].pName = "main";

		// Create Stage Info for Fragment Shader
		stage_create_info[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stage_create_info[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		stage_create_info[1].module = vulkanRenderer.skyboxFragmentShader;
		stage_create_info[1].pName = "main";

		VkPipelineInputAssemblyStateCreateInfo assembly_create_info = {};
		assembly_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		assembly_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		assembly_create_info.primitiveRestartEnable = false;

		VkPipelineVertexInputStateCreateInfo input_vertex_info = {};
		input_vertex_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		input_vertex_info.vertexBindingDescriptionCount = 0;
		input_vertex_info.pVertexBindingDescriptions = nullptr;
		input_vertex_info.vertexAttributeDescriptionCount = 0;
		input_vertex_info.pVertexAttributeDescriptions = nullptr;

		unsigned int windowWidth, windowHeight;
		win.GetClientWidth(windowWidth);
		win.GetClientHeight(windowHeight);
		VkViewport viewport = CreateViewportFromWindowDimensions(windowWidth, windowHeight);

		VkRect2D scissor = CreateScissorFromWindowDimensions(windowWidth, windowHeight);

		VkPipelineViewportStateCreateInfo viewport_create_info = {};
		viewport_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewport_create_info.viewportCount = 1;
		viewport_create_info.pViewports = &viewport;
		viewport_create_info.scissorCount = 1;
		viewport_create_info.pScissors = &scissor;

		VkPipelineRasterizationStateCreateInfo rasterization_create_info = {};
		rasterization_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterization_create_info.rasterizerDiscardEnable = VK_FALSE;
		rasterization_create_info.polygonMode = VK_POLYGON_MODE_FILL;
		rasterization_create_info.lineWidth = 1.0f;
		rasterization_create_info.cullMode = VK_CULL_MODE_NONE;
		rasterization_create_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
		rasterization_create_info.depthClampEnable = VK_FALSE;
		rasterization_create_info.depthBiasEnable = VK_FALSE;

		VkPipelineMultisampleStateCreateInfo multisample_create_info = {};
		multisample_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisample_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info = {};
		depth_stencil_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depth_stencil_create_info.depthTestEnable = VK_FALSE;
		depth_stencil_create_info.depthWriteEnable = VK_FALSE;
		depth_stencil_create_info.depthCompareOp = VK_COMPARE_OP_ALWAYS;
		depth_stencil_create_info.stencilTestEnable = VK_FALSE;

		VkPipelineColorBlendAttachmentState color_blend_attachment_state = {};
		color_blend_attachment_state.colorWriteMask = 0xF;
		color_blend_attachment_state.blendEnable = VK_FALSE;

		VkPipelineColorBlendStateCreateInfo color_blend_create_info = {};
		color_blend_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		color_blend_create_info.attachmentCount = 1;
		color_blend_create_info.pAttachments = &color_blend_attachment_state;

		// Dynamic State 
		VkDynamicState dynamic_states[2] =
		{
			// By setting these we do not need to re-create the pipeline on Resize
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};
		VkPipelineDynamicStateCreateInfo dynamic_create_info = {};
		dynamic_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamic_create_info.dynamicStateCount = 2;
		dynamic_create_info.pDynamicStates = dynamic_states;

		VkPushConstantRange push_constant_range{};
		push_constant_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		push_constant_range.offset = 0;
		push_constant_range.size = sizeof(SkyboxPush);

		VkPipelineLayoutCreateInfo pipeline_layout_create_info = {};
		pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipeline_layout_create_info.setLayoutCount = 0;
		pipeline_layout_create_info.pSetLayouts = nullptr;
		pipeline_layout_create_info.pushConstantRangeCount = 1;
		pipeline_layout_create_info.pPushConstantRanges = &push_constant_range;

		vkCreatePipelineLayout(vulkanRenderer.device, &pipeline_layout_create_info, nullptr, &vulkanRenderer.skyboxPipelineLayout);

		// Pipeline State... (FINALLY) 
		VkGraphicsPipelineCreateInfo pipeline_create_info = {};
		pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipeline_create_info.stageCount = 2;
		pipeline_create_info.pStages = stage_create_info;
		pipeline_create_info.pInputAssemblyState = &assembly_create_info;
		pipeline_create_info.pVertexInputState = &input_vertex_info;
		pipeline_create_info.pViewportState = &viewport_create_info;
		pipeline_create_info.pRasterizationState = &rasterization_create_info;
		pipeline_create_info.pMultisampleState = &multisample_create_info;
		pipeline_create_info.pDepthStencilState = &depth_stencil_create_info;
		pipeline_create_info.pColorBlendState = &color_blend_create_info;
		pipeline_create_info.pDynamicState = &dynamic_create_info;
		pipeline_create_info.layout = vulkanRenderer.skyboxPipelineLayout;
		pipeline_create_info.renderPass = vulkanRenderer.renderPass;
		pipeline_create_info.subpass = 0;
		pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;

		vkCreateGraphicsPipelines(vulkanRenderer.device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &vulkanRenderer.skyboxPipeline);
	}

	//*** SYSTEMS ***//

	// run this code when a VulkanRenderer component is connected
	void Construct_VulkanRenderer(entt::registry& registry, entt::entity entity)
	{
		if (!registry.all_of<GW::SYSTEM::GWindow>(entity))
		{
			std::cout << "Window not added to the registry yet!" << std::endl;
			abort();
			return;
		}

		if (!registry.all_of<VulkanRendererInitialization>(entity))
		{
			std::cout << "Initialization Data not added to the registry yet!" << std::endl;
			abort();
			return;
		}

		auto& vulkanRenderer = registry.get<VulkanRenderer>(entity);
		auto& initializationData = registry.get<VulkanRendererInitialization>(entity);

		registry.emplace<VulkanUIRenderer>(entity);
		auto& vulkanUIRenderer = registry.get<VulkanUIRenderer>(entity);

		registry.emplace<UI>(entity);
		registry.emplace<UIDrawList>(entity);

		GW::SYSTEM::GWindow win = registry.get<GW::SYSTEM::GWindow>(entity);		
#ifndef NDEBUG
		const char* debugLayers[] = {
			"VK_LAYER_KHRONOS_validation", // standard validation layer
		};
		if (-vulkanRenderer.vlkSurface.Create(win, GW::GRAPHICS::DEPTH_BUFFER_SUPPORT,
			sizeof(debugLayers) / sizeof(debugLayers[0]),
			debugLayers, 0, nullptr, 0, nullptr, false))
#else
		if (-vulkanRenderer.vlkSurface.Create(win, GW::GRAPHICS::DEPTH_BUFFER_SUPPORT))
#endif
		{
			std::cout << "Failed to create Vulkan Surface!" << std::endl;
			abort();
			return;
		}

		vulkanRenderer.clrAndDepth[0].color = initializationData.clearColor;
		vulkanRenderer.clrAndDepth[1].depthStencil = initializationData.depthStencil;

		// Create Projection matrix
		float aspectRatio;
		vulkanRenderer.vlkSurface.GetAspectRatio(aspectRatio);
		GW::MATH::GMatrix::ProjectionVulkanLHF(G2D_DEGREE_TO_RADIAN_F(initializationData.fovDegrees), aspectRatio, initializationData.nearPlane, initializationData.farPlane, vulkanRenderer.projMatrix);

		
		vulkanRenderer.vlkSurface.GetDevice((void**)&vulkanRenderer.device);
		vulkanRenderer.vlkSurface.GetPhysicalDevice((void**)&vulkanRenderer.physicalDevice);
		vulkanRenderer.vlkSurface.GetRenderPass((void**)&vulkanRenderer.renderPass);

		// Intialize runtime shader compiler HLSL -> SPIRV
		shaderc_compiler_t compiler = shaderc_compiler_initialize();
		shaderc_compile_options_t options = shaderc_compile_options_initialize();
		shaderc_compile_options_set_source_language(options, shaderc_source_language_hlsl);
		shaderc_compile_options_set_invert_y(options, false);
#ifndef NDEBUG
		shaderc_compile_options_set_generate_debug_info(options);
#endif

		// Vertex Shader
		std::string vertexShaderSource = ReadFileIntoString(initializationData.vertexShaderName.c_str());

		shaderc_compilation_result_t result = shaderc_compile_into_spv( // compile
			compiler, vertexShaderSource.c_str(), vertexShaderSource.length(),
			shaderc_vertex_shader, "main.vert", "main", options);

		if (shaderc_result_get_compilation_status(result) != shaderc_compilation_status_success) // errors?
		{
			std::cout << "Vertex Shader Errors : \n" << shaderc_result_get_error_message(result) << std::endl;
			abort();
			return;
		}

		GvkHelper::create_shader_module(vulkanRenderer.device, shaderc_result_get_length(result), // load into Vulkan
			(char*)shaderc_result_get_bytes(result), &vulkanRenderer.vertexShader);

		shaderc_result_release(result); // done

		// Fragment Shader
		std::string fragmentShaderSource = ReadFileIntoString(initializationData.fragmentShaderName.c_str());

		result = shaderc_compile_into_spv( // compile
			compiler, fragmentShaderSource.c_str(), fragmentShaderSource.length(),
			shaderc_fragment_shader, "main.frag", "main", options);

		if (shaderc_result_get_compilation_status(result) != shaderc_compilation_status_success) // errors?
		{
			std::cout << "Fragment Shader Errors : \n" << shaderc_result_get_error_message(result) << std::endl;
			abort();
			return;
		}

		GvkHelper::create_shader_module(vulkanRenderer.device, shaderc_result_get_length(result), // load into Vulkan
			(char*)shaderc_result_get_bytes(result), &vulkanRenderer.fragmentShader);

		shaderc_result_release(result); // done

		//UI Vertex Shader

		std::string uiVertexShaderSource = ReadFileIntoString(initializationData.uiVertexShaderName.c_str());

		result = shaderc_compile_into_spv( // compile
			compiler, uiVertexShaderSource.c_str(), uiVertexShaderSource.length(),
			shaderc_vertex_shader, "main.vert", "main", options);

		if (shaderc_result_get_compilation_status(result) != shaderc_compilation_status_success) // errors?
		{
			std::cout << "UI Vertex Shader Errors : \n" << shaderc_result_get_error_message(result) << std::endl;
			abort();
			return;
		}

		GvkHelper::create_shader_module(vulkanRenderer.device, shaderc_result_get_length(result), // load into Vulkan
			(char*)shaderc_result_get_bytes(result), &vulkanUIRenderer.vertexShader);

		shaderc_result_release(result); // done

		// UI Fragment Shader
		std::string uiFragmentShaderSource = ReadFileIntoString(initializationData.uiFragmentShaderName.c_str());

		result = shaderc_compile_into_spv( // compile
			compiler, uiFragmentShaderSource.c_str(), uiFragmentShaderSource.length(),
			shaderc_fragment_shader, "main.frag", "main", options);

		if (shaderc_result_get_compilation_status(result) != shaderc_compilation_status_success) // errors?
		{
			std::cout << "UI Fragment Shader Errors : \n" << shaderc_result_get_error_message(result) << std::endl;
			abort();
			return;
		}

		GvkHelper::create_shader_module(vulkanRenderer.device, shaderc_result_get_length(result), // load into Vulkan
			(char*)shaderc_result_get_bytes(result), &vulkanUIRenderer.fragmentShader);

		shaderc_result_release(result); // done

		//Skybox Vertex Shader

		std::string skyboxVertexShaderSource = ReadFileIntoString(initializationData.skyboxVertexShaderName.c_str());

		result = shaderc_compile_into_spv( // compile
			compiler, skyboxVertexShaderSource.c_str(), skyboxVertexShaderSource.length(),
			shaderc_vertex_shader, "main.vert", "main", options);

		if (shaderc_result_get_compilation_status(result) != shaderc_compilation_status_success) // errors?
		{
			std::cout << "Skybox Vertex Shader Errors : \n" << shaderc_result_get_error_message(result) << std::endl;
			abort();
			return;
		}

		GvkHelper::create_shader_module(vulkanRenderer.device, shaderc_result_get_length(result), // load into Vulkan
			(char*)shaderc_result_get_bytes(result), &vulkanRenderer.skyboxVertexShader);

		shaderc_result_release(result);

		//Skybox Fragment Shader

		std::string skyboxFragmentShaderSource = ReadFileIntoString(initializationData.skyboxFragmentShaderName.c_str());

		result = shaderc_compile_into_spv( // compile
			compiler, skyboxFragmentShaderSource.c_str(), skyboxFragmentShaderSource.length(),
			shaderc_fragment_shader, "main.frag", "main", options);

		if (shaderc_result_get_compilation_status(result) != shaderc_compilation_status_success) // errors?
		{
			std::cout << "Skybox Fragment Shader Errors : \n" << shaderc_result_get_error_message(result) << std::endl;
			abort();
			return;
		}

		GvkHelper::create_shader_module(vulkanRenderer.device, shaderc_result_get_length(result), // load into Vulkan
			(char*)shaderc_result_get_bytes(result), &vulkanRenderer.skyboxFragmentShader);

		shaderc_result_release(result); // done

		// Free runtime shader compiler resources
		shaderc_compile_options_release(options);
		shaderc_compiler_release(compiler);

		InitializeGraphicsPipeline(registry, entity);
		InitializeUIGraphicsPipeline(registry, entity);
		InitializeSkyboxGraphicsPipeline(registry, entity);
		registry.emplace<VulkanUIVertexBuffer>(entity);
		registry.emplace<VulkanUIIndexBuffer>(entity);
		// Remove the initializtion data as we no longer need it
		registry.remove<VulkanRendererInitialization>(entity);

	}

	// run this code when a VulkanRenderer component is updated
	void Update_VulkanRenderer(entt::registry& registry, entt::entity entity)
	{
		auto& vulkanRenderer = registry.get<VulkanRenderer>(entity);

		if (-vulkanRenderer.vlkSurface.StartFrame(2, vulkanRenderer.clrAndDepth))
		{
			std::cout << "Failed to start frame!" << std::endl;
			return;
		}

		UTIL::DeltaTime& deltaTime = registry.ctx().get<UTIL::DeltaTime>();
		vulkanRenderer.timeSeconds += deltaTime.dtSec;

		if (registry.any_of<CPULevel>(entity)) {
			auto* cpuLevel = registry.try_get<CPULevel>(entity);
			int count = cpuLevel != nullptr ? cpuLevel->levelData.levelMaterials.size() : 0;
			if (!vulkanRenderer.materialsLoaded || vulkanRenderer.cachedMaterialCount != count) {
				LoadMaterials(registry, entity);
				vulkanRenderer.materialsLoaded = true;
				vulkanRenderer.cachedMaterialCount = count;
			}
			
		}

		auto win = registry.get<GW::SYSTEM::GWindow>(entity);
		unsigned int frame;
		vulkanRenderer.vlkSurface.GetSwapchainCurrentImage(frame);

		VkCommandBuffer commandBuffer;
		unsigned int currentBuffer;
		vulkanRenderer.vlkSurface.GetSwapchainCurrentImage(currentBuffer);
		vulkanRenderer.vlkSurface.GetCommandBuffer(currentBuffer, (void**)&commandBuffer);

		unsigned int windowWidth, windowHeight;
		win.GetClientWidth(windowWidth);
		win.GetClientHeight(windowHeight);
		VkViewport viewport = CreateViewportFromWindowDimensions(windowWidth, windowHeight);
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

		VkRect2D scissor = CreateScissorFromWindowDimensions(windowWidth, windowHeight);
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkanRenderer.skyboxPipeline);
		SkyboxPush push = { {0.1,0.3,1,1}, {0,0,0.1,vulkanRenderer.timeSeconds} };
		vkCmdPushConstants(commandBuffer, vulkanRenderer.skyboxPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SkyboxPush), &push);
		vkCmdDraw(commandBuffer, 3, 1, 0, 0);

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkanRenderer.pipeline);

		// Update uniform and storage buffers
		registry.patch<VulkanUniformBuffer>(entity);

		// TODO: Build draw instructions

		// Check for presence of the buffers first as they take a few frames before they are created
		if (registry.all_of< VulkanVertexBuffer, VulkanIndexBuffer>(entity))
		{
			auto& vertexBuffer = registry.get<VulkanVertexBuffer>(entity);
			auto& indexBuffer = registry.get<VulkanIndexBuffer>(entity);

			if (vertexBuffer.buffer != VK_NULL_HANDLE && indexBuffer.buffer != VK_NULL_HANDLE)
			{
				VkDeviceSize offsets[] = { 0 };
				vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer.buffer, offsets);
				vkCmdBindIndexBuffer(commandBuffer, indexBuffer.buffer, 0, VkIndexType::VK_INDEX_TYPE_UINT32);
			}

			entt::basic_group instances = registry.group(entt::get<GeometryData, GPUInstance>, entt::exclude<DoNotRender>);
			instances.sort<GeometryData>([] (const GeometryData& a, const GeometryData& b) {
				return a < b;
			});

			std::vector<GPUInstance> gpuInstanceList;
			std::map<GeometryData, int> geometryDataMap;

			for (int i = 0; i < instances.size(); i++) {
				entt::entity instance = instances[i];
				GPUInstance gpuInstance = registry.get<GPUInstance>(instance);
				GeometryData geometryData = registry.get<GeometryData>(instance);
				gpuInstanceList.push_back(gpuInstance);
				if (geometryDataMap.find(geometryData) == geometryDataMap.end()) {
					geometryDataMap.insert({geometryData, 0});
				}
				geometryDataMap[geometryData]++;
			}

			registry.emplace_or_replace<std::vector<GPUInstance>>(entity, std::move(gpuInstanceList));
			
			registry.patch<VulkanGPUInstanceBuffer>(entity);

			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkanRenderer.pipelineLayout, 0, 1, &vulkanRenderer.descriptorSets[frame], 0, nullptr);
			
			std::map<GeometryData, int>::iterator iter;
			int totalInstances = 0;
			VkDescriptorSet boundMaterialSet = VK_NULL_HANDLE;
			for (iter = geometryDataMap.begin(); iter != geometryDataMap.end(); ++iter) {
				const GeometryData& geometryData = iter->first;
				if (!vulkanRenderer.materialSets.empty()) {
					VkDescriptorSet materialDescriptorSet = vulkanRenderer.materialSets[geometryData.materialIndex];
					if (materialDescriptorSet != boundMaterialSet) {
						vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkanRenderer.pipelineLayout, 1, 1, &materialDescriptorSet, 0, nullptr);
						boundMaterialSet = materialDescriptorSet;
					}
				}
				vkCmdDrawIndexed(commandBuffer, geometryData.indexCount, iter->second, geometryData.indexStart, geometryData.vertexStart, totalInstances);
				totalInstances += iter->second;
			}
			
		}

		if (registry.all_of<VulkanUIRenderer, VulkanUIVertexBuffer, VulkanUIIndexBuffer>(entity)) {

			registry.patch<UI>(entity);
			auto& uiDrawList  = registry.get<UIDrawList>(entity);
			
				
			registry.patch<VulkanUIVertexBuffer>(entity);
			registry.patch<VulkanUIIndexBuffer>(entity);

			auto& uiRenderer = registry.get<VulkanUIRenderer>(entity);
			auto& uiVertexBuffer = registry.get<VulkanUIVertexBuffer>(entity);
			auto& uiIndexBuffer = registry.get<VulkanUIIndexBuffer>(entity);

			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, uiRenderer.pipeline);

			UIPush uiPush{ {(float)windowWidth, (float)windowHeight} };
			vkCmdPushConstants(commandBuffer, uiRenderer.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(UIPush), &uiPush);

			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, &uiVertexBuffer.buffer[frame], offsets);
			vkCmdBindIndexBuffer(commandBuffer, uiIndexBuffer.buffer[frame], 0, VK_INDEX_TYPE_UINT32);

			VkDescriptorSet boundSet = VK_NULL_HANDLE;

			for (auto& command : uiDrawList.drawCommands) {
				vkCmdSetScissor(commandBuffer, 0, 1, &command.clip);
				auto& texture = uiRenderer.textures[command.textureId];
				if (texture.set != boundSet) {
					vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, uiRenderer.pipelineLayout, 0, 1, &texture.set, 0, nullptr);
					boundSet = texture.set;
				}
				
				vkCmdDrawIndexed(commandBuffer, command.indexCount, 1, command.firstIndex, command.vertexOffset, 0);
			}
		}

		vulkanRenderer.vlkSurface.EndFrame(true);
	}

	// run this code when a VulkanRenderer component is updated
	void Destroy_VulkanRenderer(entt::registry& registry, entt::entity entity)
	{
		auto& vulkanRenderer = registry.get<VulkanRenderer>(entity);
		
		VkDevice device = VK_NULL_HANDLE;
		if ( !( +vulkanRenderer.vlkSurface.GetDevice((void**)&device) ) || device == VK_NULL_HANDLE) {
			return;
		}

		// wait till everything has completed
		vkDeviceWaitIdle(vulkanRenderer.device);
		// Remove Buffer compontents
		registry.remove<VulkanIndexBuffer>(entity);
		registry.remove<VulkanVertexBuffer>(entity);
		registry.remove<VulkanUIIndexBuffer>(entity);
		registry.remove<VulkanUIVertexBuffer>(entity);
		registry.remove<VulkanGPUInstanceBuffer>(entity);
		registry.remove<VulkanUniformBuffer>(entity);


		vkDestroyDescriptorSetLayout(vulkanRenderer.device, vulkanRenderer.descriptorLayout, nullptr);
		vkDestroyDescriptorPool(vulkanRenderer.device, vulkanRenderer.descriptorPool, nullptr);

		
		if (registry.all_of<VulkanUIRenderer>(entity)) {
			auto& vulkanUIRenderer = registry.get<VulkanUIRenderer>(entity);
			vkDestroyDescriptorSetLayout(vulkanRenderer.device, vulkanUIRenderer.descriptorSetLayout, nullptr);
			vkDestroyDescriptorPool(vulkanRenderer.device, vulkanUIRenderer.descriptorPool, nullptr);
			for (auto& tex : vulkanUIRenderer.textures) {
				DestroyUITexture(vulkanRenderer, tex);
			}
			vkDestroyShaderModule(vulkanRenderer.device, vulkanUIRenderer.vertexShader, nullptr);
			vkDestroyShaderModule(vulkanRenderer.device, vulkanUIRenderer.fragmentShader, nullptr);
			vkDestroyPipelineLayout(vulkanRenderer.device, vulkanUIRenderer.pipelineLayout, nullptr);
			vkDestroyPipeline(vulkanRenderer.device, vulkanUIRenderer.pipeline, nullptr);

			registry.remove<UI>(entity);
			registry.remove<UIDrawList>(entity);
			registry.remove<VulkanUIRenderer>(entity);

		}

		vkDestroySampler(vulkanRenderer.device, vulkanRenderer.materialSampler, nullptr);
		vkDestroyDescriptorPool(vulkanRenderer.device, vulkanRenderer.materialPool, nullptr);
		vkDestroyDescriptorSetLayout(vulkanRenderer.device, vulkanRenderer.materialSetLayout, nullptr);

		for (auto& texture : vulkanRenderer.textures) {
			vkDestroyImageView(vulkanRenderer.device, texture.imageView, nullptr);
			vkDestroyImage(vulkanRenderer.device, texture.image, nullptr);
			vkFreeMemory(vulkanRenderer.device, texture.memory, nullptr);
		}

		vulkanRenderer.textures.clear();
		vulkanRenderer.textureCache.clear();

		// Release allocated shaders & pipeline
		vkDestroyShaderModule(vulkanRenderer.device, vulkanRenderer.vertexShader, nullptr);
		vkDestroyShaderModule(vulkanRenderer.device, vulkanRenderer.fragmentShader, nullptr);
		vkDestroyShaderModule(vulkanRenderer.device, vulkanRenderer.skyboxVertexShader, nullptr);
		vkDestroyShaderModule(vulkanRenderer.device, vulkanRenderer.skyboxFragmentShader, nullptr);
		vkDestroyPipelineLayout(vulkanRenderer.device, vulkanRenderer.skyboxPipelineLayout, nullptr);
		vkDestroyPipelineLayout(vulkanRenderer.device, vulkanRenderer.pipelineLayout, nullptr);
		vkDestroyPipeline(vulkanRenderer.device, vulkanRenderer.skyboxPipeline, nullptr);
		vkDestroyPipeline(vulkanRenderer.device, vulkanRenderer.pipeline, nullptr);
	}

	// Use this MACRO to connect the EnTT Component Logic
	CONNECT_COMPONENT_LOGIC() {
		// register the Window component's logic
		registry.on_construct<VulkanRenderer>().connect<Construct_VulkanRenderer>();
		registry.on_update<VulkanRenderer>().connect<Update_VulkanRenderer>();
		registry.on_destroy<VulkanRenderer>().connect<Destroy_VulkanRenderer>();
	}

} // namespace DRAW