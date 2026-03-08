#include "DrawComponents.h"
#include "../CCL.h"

namespace DRAW {
	void Update_UIDrawList(entt::registry& registry, entt::entity entity) {
		auto& uiDrawList = registry.get<UIDrawList>(entity);

		uiDrawList.vertices.clear();
		uiDrawList.indices.clear();
		uiDrawList.drawCommands.clear();
	}

	CONNECT_COMPONENT_LOGIC() {
		registry.on_update<UIDrawList>().connect<Update_UIDrawList>();
	}
}