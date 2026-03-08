#include "DrawComponents.h"
#include "../CCL.h"
#include "../GAME/GameComponents.h"

namespace DRAW {
	void Destroy_MeshCollection(entt::registry& registry, entt::entity entity) {
		MeshCollection& meshCollection = registry.get<MeshCollection>(entity);
		for (auto child : meshCollection.entities) {
			if (!registry.valid(child)) continue;
			registry.emplace_or_replace<GAME::ToDestroy>(child);
		}
		meshCollection.entities.clear();
	}

	CONNECT_COMPONENT_LOGIC() {
		registry.on_destroy<MeshCollection>().connect<Destroy_MeshCollection>();
	}
}