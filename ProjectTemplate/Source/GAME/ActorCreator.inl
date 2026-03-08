#include "ActorCreator.h"
#include "GameComponents.h"
#include "../DRAW/DrawComponents.h"
#include "../UTIL/GameConfig.h"

namespace ActorCreator {
	/// <summary>
	/// Utility to create "dynamic" entities that have a model and transform
	/// </summary>
	/// <typeparam name="...T">A list of components to attach to the entity</typeparam>
	/// <param name="registry">EnTT's registry</param>
	/// <param name="config">A reference to the config emplaced on the registry's context</param>
	/// <param name="configSection">The config section to use for the model</param>
	/// <param name="transformMatrix">The initial transform to use. Defaults to an identity matrix with INFINITY on data[0]</param>
	/// <returns>The newly created entity</returns>
	template<typename... T>
	entt::entity ConstructActor(entt::registry& registry, std::shared_ptr<const GameConfig> config, std::string configSection, GW::MATH::GMATRIXF transformMatrix) {

		entt::entity entity = registry.create();

		registry.emplace<GAME::ConfigSection>(entity, configSection);
		registry.emplace<GAME::Transform>(entity);
		registry.emplace<DRAW::MeshCollection>(entity);

		(registry.emplace<T>(entity), ...);

		DRAW::ModelManager& modelManager = registry.ctx().get<DRAW::ModelManager>();
		std::string modelName = (*config).at(configSection).at("model").as<std::string>();
		auto& meshes = modelManager.meshCollections[modelName];
		auto& meshCollection = registry.get<DRAW::MeshCollection>(entity);
		meshCollection.collider = meshes[0].collider;
		
		for (int n = 0; n < meshes.size(); n++) {
			unsigned int size = meshes[n].entities.size();
			for (unsigned int i = 0; i < size; i++) {
				auto proto = meshes[n].entities[i];
				
				entt::entity meshEntity = registry.create();

				DRAW::GPUInstance gpuInstance = registry.get<DRAW::GPUInstance>(meshes[n].entities[i]);

				DRAW::GeometryData geometryData = registry.get<DRAW::GeometryData>(meshes[n].entities[i]);

				if (transformMatrix.data[0] != INFINITY) {
					gpuInstance.transform = transformMatrix;
				}

				registry.emplace<DRAW::GPUInstance>(meshEntity, gpuInstance);
				registry.emplace<DRAW::GeometryData>(meshEntity, geometryData);
				meshCollection.entities.push_back(meshEntity);
				if (i == 0) {
					GAME::Transform& transform = registry.get<GAME::Transform>(entity);
					if (transformMatrix.data[0] != INFINITY) {
						transform.matrix = transformMatrix;
					}
					else {
						transform.matrix = gpuInstance.transform;
					}
					
				}
			}
		}

		return entity;
	}
}