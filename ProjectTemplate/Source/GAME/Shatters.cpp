#include "GameComponents.h"
#include "../CCL.h"
#include "../UTIL/Utilities.h"
#include "ActorCreator.h"


namespace GAME {

	void Update_Shatters(entt::registry& registry, entt::entity entity) {
		Shatters* shatters = registry.try_get<Shatters>(entity);
		ConfigSection* configSection = registry.try_get<ConfigSection>(entity);
		if (shatters == nullptr || configSection == nullptr) return;
		std::shared_ptr<const GameConfig> config = registry.ctx().get<UTIL::Config>().gameConfig;
		for (int i = 0; i < shatters->spawnCount; i++) {
			GW::MATH::GMATRIXF newMatrix;
			GW::MATH::GMatrix::ScaleGlobalF(registry.get<Transform>(entity).matrix, GW::MATH::GVECTORF{ shatters->scale, shatters->scale, shatters->scale, 1 }, newMatrix);
			entt::entity enemy = ActorCreator::ConstructActor<GAME::Enemy, GAME::Velocity, GAME::Health>(registry, config, configSection->configSection, newMatrix);

			GAME::SetDirection(registry, enemy, UTIL::GetRandomVelocityVector());

			if (shatters->shattersRemaining - 1 > 0) {
				registry.emplace<Shatters>(enemy);
				registry.get<Shatters>(enemy).shattersRemaining = shatters->shattersRemaining - 1;
			}

			registry.emplace<Collidable>(enemy);
		}
	}

	void Construct_Shatters(entt::registry& registry, entt::entity entity) {
		Shatters* shatters = registry.try_get<Shatters>(entity);
		ConfigSection* configSection = registry.try_get<ConfigSection>(entity);
		if (shatters == nullptr || configSection == nullptr) return;

		std::shared_ptr<const GameConfig> config = registry.ctx().get<UTIL::Config>().gameConfig;
		shatters->shattersRemaining = (*config).at(configSection->configSection).at("initialShatterCount").as<int>();
		shatters->spawnCount = (*config).at(configSection->configSection).at("shatterAmount").as<int>();
		shatters->scale = (*config).at(configSection->configSection).at("shatterScale").as<float>();
	}



	CONNECT_COMPONENT_LOGIC() {
		registry.on_update<Shatters>().connect<Update_Shatters>();
		registry.on_construct<Shatters>().connect<Construct_Shatters>();
	}
}