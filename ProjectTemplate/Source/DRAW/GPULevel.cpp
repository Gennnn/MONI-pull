#include "DrawComponents.h"
#include "../GAME/GameComponents.h"
#include "../CCL.h"

namespace DRAW {
	void Construct_GPULevel(entt::registry& registry, entt::entity entity) {

		CPULevel* cpuLevelComponent = registry.try_get<CPULevel>(entity);
		if (cpuLevelComponent == nullptr) return;

		VulkanIndexBuffer& indexBuffer = registry.emplace<VulkanIndexBuffer>(entity);
		VulkanVertexBuffer& vertexBuffer = registry.emplace<VulkanVertexBuffer>(entity);
		registry.emplace<std::vector<H2B::VERTEX>>(entity, (*cpuLevelComponent).levelData.levelVertices);
		registry.emplace<std::vector<unsigned int>>(entity, (*cpuLevelComponent).levelData.levelIndices);

		registry.patch<VulkanIndexBuffer>(entity);
		registry.patch<VulkanVertexBuffer>(entity);

		Level_Data& levelData = cpuLevelComponent->levelData;
		ModelManager& modelManager = registry.ctx().get<ModelManager>();

		std::vector<entt::entity> covers;
		std::vector<int> coverIDS;

		for (int i = 0; i < levelData.blenderObjects.size(); i++) {

			Level_Data::BLENDER_OBJECT blenderObject = levelData.blenderObjects[i];
			Level_Data::LEVEL_MODEL levelModel = levelData.levelModels[blenderObject.modelIndex];

			MeshCollection meshCollection;

			for (int j = 0; j < levelModel.meshCount; j++) {
				
				H2B::MESH mesh = levelData.levelMeshes[levelModel.meshStart + j];
				entt::entity meshEntity = registry.create();

				GeometryData& geometryData = registry.emplace<GeometryData>(meshEntity);
				GPUInstance& gpuInstance = registry.emplace<GPUInstance>(meshEntity);

				geometryData.indexCount = mesh.drawInfo.indexCount;
				geometryData.indexStart = mesh.drawInfo.indexOffset + levelModel.indexStart;
				geometryData.vertexStart = levelModel.vertexStart;
				geometryData.materialIndex = mesh.materialIndex + levelModel.materialStart;

				gpuInstance.transform = levelData.levelTransforms[blenderObject.transformIndex];
				gpuInstance.matData = levelData.levelMaterials[mesh.materialIndex + levelModel.materialStart].attrib;

				if (levelModel.isDynamic && !levelModel.isDoor) {
					registry.emplace<DoNotRender>(meshEntity);
					meshCollection.entities.push_back(meshEntity);
				}
				

				meshCollection.collider = levelData.levelColliders[levelModel.colliderIndex];
			}

			if (!meshCollection.entities.empty()) {
				//assert(blenderObject.blendername != nullptr);
				if (modelManager.meshCollections.find(blenderObject.blendername) == modelManager.meshCollections.end()) {
					modelManager.meshCollections.insert({ blenderObject.blendername, {meshCollection} });
				}
				else {
					modelManager.meshCollections[blenderObject.blendername].push_back(meshCollection);
				}
			}

			if (levelModel.isCollidable) {
				entt::entity newCollidable = registry.create();
				registry.emplace<GAME::Collidable>(newCollidable);
				registry.emplace<MeshCollection>(newCollidable, meshCollection);
				registry.emplace<GAME::Transform>(newCollidable);
				registry.get<GAME::Transform>(newCollidable).matrix = levelData.levelTransforms[blenderObject.transformIndex];
					

				if (!levelModel.isDynamic) {
					
					registry.emplace<GAME::Obstacle>(newCollidable);
					
				}
				
				
			}

			if (levelModel.cover) {
				entt::entity newCover = registry.create();
				registry.emplace<MeshCollection>(newCover, meshCollection);
				registry.emplace<GAME::Transform>(newCover);
				registry.get<GAME::Transform>(newCover).matrix = levelData.levelTransforms[blenderObject.transformIndex];
				registry.emplace<GAME::Cover>(newCover, levelModel.level);
				covers.push_back(newCover);
				coverIDS.push_back(levelModel.level);
			}


			if (levelModel.base) {

				auto& lvlManager = registry.view<GAME::LevelManager>();
				if (lvlManager.empty()) {
					entt::entity levelManager = registry.create();
					registry.emplace<GAME::LevelManager>(levelManager);
				}


				
				for (auto ent : lvlManager) {
					GAME::LevelManager& levelManager = registry.get<GAME::LevelManager>(ent);

					GAME::Level hubLevel;
					hubLevel.camPosition = levelData.levelTransforms[blenderObject.transformIndex];
					if (levelModel.level <= 0) {
						hubLevel.camPosition.row4.y = 135;
					}
					else {

						hubLevel.camPosition.row4.y = 45;
					}
					hubLevel.camPosition.row4.z -= 7.5;
					hubLevel.levelNum = levelModel.level;
					
					levelManager.allLevels.push_back(hubLevel);
				}

				
			}

			if (levelModel.isDoor) {
				entt::entity newDoor = registry.create();
				GAME::Door doorComponent{levelModel.ID, levelModel.direction, levelModel.level};
				registry.emplace<GAME::Door>(newDoor, doorComponent);
				registry.emplace<GAME::Active>(newDoor);
				registry.emplace<GAME::Collidable>(newDoor);
				registry.emplace<MeshCollection>(newDoor, meshCollection);
				registry.emplace<GAME::Transform>(newDoor);
				
				registry.get<GAME::Transform>(newDoor).matrix = levelData.levelTransforms[blenderObject.transformIndex];
			}

			/*if (levelModel.treasure) {
				entt::entity newTreasure = registry.create();
				registry.emplace<GAME::Treasure>(newTreasure);
				registry.emplace<GAME::Collidable>(newTreasure);
				registry.emplace<MeshCollection>(newTreasure, meshCollection);
				registry.emplace<GAME::Transform>(newTreasure);
				registry.get<GAME::Transform>(newTreasure).matrix = levelData.levelTransforms[blenderObject.transformIndex];
			}*/
		}
		auto& lvlManager = registry.view<GAME::LevelManager>();
		for (auto ent : lvlManager) {
			GAME::LevelManager& levelManager = registry.get<GAME::LevelManager>(ent);
			for (int i = 0; i < covers.size(); i++) {
				levelManager.covers.resize(levelManager.allLevels.size());
				levelManager.covers[coverIDS[i]] = covers[i];
				
			}
		}
	}

	CONNECT_COMPONENT_LOGIC() {
		registry.on_construct<GPULevel>().connect<Construct_GPULevel>();
	}
}
