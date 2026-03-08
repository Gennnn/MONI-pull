#include "DrawComponents.h"
#include "../CCL.h"

namespace DRAW {
	void Destroy_ModelManager(entt::registry& registry, entt::entity entity) {

	}

	CONNECT_COMPONENT_LOGIC() {
		registry.on_destroy<ModelManager>().connect<Destroy_ModelManager>();
	}
}