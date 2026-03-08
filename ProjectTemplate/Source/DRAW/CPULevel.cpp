#include "DrawComponents.h"
#include "../CCL.h"

namespace DRAW {
	void Construct_CPULevel(entt::registry& registry, entt::entity entity) {
		auto& cpuLevelComponent = registry.get<CPULevel>(entity);

		GW::SYSTEM::GLog gLog;
		gLog.Create("Log");

		bool success = cpuLevelComponent.levelData.LoadLevel(cpuLevelComponent.levelPath.c_str(), cpuLevelComponent.modelAssetPath.c_str(), gLog);
	}

	CONNECT_COMPONENT_LOGIC() {
		registry.on_construct<CPULevel>().connect<Construct_CPULevel>();
	}
}
