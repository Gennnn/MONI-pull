#include "GameComponents.h"
#include "../CCL.h"
#include "../UTIL/Utilities.h"

namespace GAME {

	// Raymond's addition
	// Pickup logic is handled in GameManager collision checks.
	void Update_Treasure(entt::registry& registry, entt::entity entity)
	{
		
	}

	void Construct_Treasure(entt::registry& registry, entt::entity entity) {
		Treasure* treasure = registry.try_get<Treasure>(entity);
		ConfigSection* configSection = registry.try_get<ConfigSection>(entity);

		if (treasure == nullptr || configSection == nullptr) return;

		std::shared_ptr<const GameConfig> config = registry.ctx().get<UTIL::Config>().gameConfig;
		treasure->id = (*config).at(configSection->configSection).at("ID").as<int>();
		treasure->name = (*config).at(configSection->configSection).at("Name").as<std::string>();
	}

	// Raymond's addition
	CONNECT_COMPONENT_LOGIC()
	{
		registry.on_construct<Treasure>().connect<Construct_Treasure>();
		registry.on_update<Treasure>().connect<Update_Treasure>();
	}
}
