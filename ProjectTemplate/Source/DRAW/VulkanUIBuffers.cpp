#include "DrawComponents.h"
#include "../CCL.h"

namespace DRAW {
	void Construct_VulkanUIVertexBuffer(entt::registry& registry, entt::entity entity) {
		auto& uiVertexBuffer = registry.get<VulkanUIVertexBuffer>(entity);
		auto& renderer = registry.get<VulkanRenderer>(entity);

		unsigned int frameCount;
		renderer.vlkSurface.GetSwapchainImageCount(frameCount);
		uiVertexBuffer.buffer.resize(frameCount);
		uiVertexBuffer.memory.resize(frameCount);

		for (unsigned int i = 0; i < frameCount; i++) {
			GvkHelper::create_buffer(
				renderer.physicalDevice,
				renderer.device,
				sizeof(UIVertex) * uiVertexBuffer.element_count,
				VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				&uiVertexBuffer.buffer[i],
				&uiVertexBuffer.memory[i]);
		}
	}

	void Construct_VulkanUIIndexBuffer(entt::registry& registry, entt::entity entity) {
		auto& uiIndexBuffer = registry.get<VulkanUIIndexBuffer>(entity);
		auto& renderer = registry.get<VulkanRenderer>(entity);

		unsigned int frameCount;
		renderer.vlkSurface.GetSwapchainImageCount(frameCount);
		uiIndexBuffer.buffer.resize(frameCount);
		uiIndexBuffer.memory.resize(frameCount);

		for (unsigned int i = 0; i < frameCount; i++) {
			GvkHelper::create_buffer(
				renderer.physicalDevice,
				renderer.device, 
				sizeof(unsigned int) * uiIndexBuffer.element_count,
				VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				&uiIndexBuffer.buffer[i],
				&uiIndexBuffer.memory[i]);
		}

	}

	void Destroy_VulkanUIVertexBuffer(entt::registry& registry, entt::entity entity) {
		auto& uiVertexBuffer = registry.get<VulkanUIVertexBuffer>(entity);
		auto& renderer = registry.get<VulkanRenderer>(entity);

		for (auto buffer : uiVertexBuffer.buffer) {
			vkDestroyBuffer(renderer.device, buffer, nullptr);
		}
		for (auto memory : uiVertexBuffer.memory) {
			vkFreeMemory(renderer.device, memory, nullptr);
		}
	}

	void Destroy_VulkanUIIndexBuffer(entt::registry& registry, entt::entity entity) {
		auto& uiIndexBuffer = registry.get<VulkanUIIndexBuffer>(entity);
		auto& renderer = registry.get<VulkanRenderer>(entity);

		for (auto buffer : uiIndexBuffer.buffer) {
			vkDestroyBuffer(renderer.device, buffer, nullptr);
		}
		for (auto memory : uiIndexBuffer.memory) {
			vkFreeMemory(renderer.device, memory, nullptr);
		}
	}

	void Update_VulkanUIVertexBuffer(entt::registry& registry, entt::entity entity) {
		if (!registry.all_of<UIDrawList>(entity)) return;

		auto& uiVertexBuffer = registry.get<VulkanUIVertexBuffer>(entity);
		auto& uiDrawList = registry.get<UIDrawList>(entity);
		auto& renderer = registry.get<VulkanRenderer>(entity);

		if (uiDrawList.vertices.size() > uiVertexBuffer.element_count) {
			Destroy_VulkanUIVertexBuffer(registry, entity);
			while (uiDrawList.vertices.size() > uiVertexBuffer.element_count) {
				uiVertexBuffer.element_count *= 2;
			}

			for (unsigned int i = 0; i < uiVertexBuffer.buffer.size(); i++) {
				GvkHelper::create_buffer(
					renderer.physicalDevice,
					renderer.device, 
					sizeof(UIVertex) * uiVertexBuffer.element_count,
					VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					&uiVertexBuffer.buffer[i],
					&uiVertexBuffer.memory[i]);
			}
		}

		unsigned int currentFrame;
		renderer.vlkSurface.GetSwapchainCurrentImage(currentFrame);

		if (!uiDrawList.vertices.empty()) {
			GvkHelper::write_to_buffer(
				renderer.device,
				uiVertexBuffer.memory[currentFrame],
				uiDrawList.vertices.data(),
				sizeof(UIVertex) * uiDrawList.vertices.size());
		}
	}

	void Update_VulkanUIIndexBuffer(entt::registry& registry, entt::entity entity) {
		if (!registry.all_of<UIDrawList>(entity)) return;

		auto& uiIndexBuffer = registry.get<VulkanUIIndexBuffer>(entity);
		auto& uiDrawList = registry.get<UIDrawList>(entity);
		auto& renderer = registry.get<VulkanRenderer>(entity);

		if (uiDrawList.indices.size() > uiIndexBuffer.element_count) {
			Destroy_VulkanUIIndexBuffer(registry, entity);
			while (uiDrawList.indices.size() > uiIndexBuffer.element_count) {
				uiIndexBuffer.element_count *= 2;
			}

			for (unsigned int i = 0; i < uiIndexBuffer.buffer.size(); i++) {
				GvkHelper::create_buffer(
					renderer.physicalDevice,
					renderer.device,
					sizeof(unsigned int) * uiIndexBuffer.element_count,
					VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					&uiIndexBuffer.buffer[i],
					&uiIndexBuffer.memory[i]);
			}
		}

		unsigned int currentFrame;
		renderer.vlkSurface.GetSwapchainCurrentImage(currentFrame);

		if (!uiDrawList.indices.empty()) {
			GvkHelper::write_to_buffer(
				renderer.device,
				uiIndexBuffer.memory[currentFrame],
				uiDrawList.indices.data(),
				sizeof(unsigned int) * uiDrawList.indices.size());
		}
	}

	CONNECT_COMPONENT_LOGIC() {
		registry.on_construct<VulkanUIVertexBuffer>().connect<Construct_VulkanUIVertexBuffer>();
		registry.on_update<VulkanUIVertexBuffer>().connect<Update_VulkanUIVertexBuffer>();
		registry.on_destroy<VulkanUIVertexBuffer>().connect<Destroy_VulkanUIVertexBuffer>();

		registry.on_construct<VulkanUIIndexBuffer>().connect<Construct_VulkanUIIndexBuffer>();
		registry.on_update<VulkanUIIndexBuffer>().connect<Update_VulkanUIIndexBuffer>();
		registry.on_destroy<VulkanUIIndexBuffer>().connect<Destroy_VulkanUIIndexBuffer>();

		
	}
}