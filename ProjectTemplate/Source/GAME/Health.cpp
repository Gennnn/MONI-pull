#include "GameComponents.h"
#include "../CCL.h"
#include "../UTIL/Utilities.h"


namespace GAME {
	// Decreases the health of an entity. Returns the new health amount, or -1 if the entity does not have a Health component. - E.A.
	int ModifyHealth(entt::registry& registry, entt::entity entity, int amount) {
		
		Health* health = registry.try_get<Health>(entity);
		if (health == nullptr) return -1;

		if (registry.all_of<GAME::Invuln>(entity)) return health->amount;
		health->amount -= amount;
		health->amount = max(0, health->amount);
		return health->amount;
	}

	/// <summary>
	/// Automatic setup for starting health called on Health component construction
	/// </summary>
	void Construct_Health(entt::registry& registry, entt::entity entity) {
		Health* health = registry.try_get<Health>(entity);
		ConfigSection* configSection = registry.try_get<ConfigSection>(entity);
		if (configSection == nullptr) return;
		std::shared_ptr<const GameConfig> config = registry.ctx().get<UTIL::Config>().gameConfig;
		health->amount = (*config).at(configSection->configSection).at("hitpoints").as<int>();
	}


	CONNECT_COMPONENT_LOGIC() {
		registry.on_construct<Health>().connect<Construct_Health>();
	}
}