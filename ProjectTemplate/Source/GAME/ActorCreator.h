#ifndef ACTORCREATOR_H
#define ACTORCREATOR_H

#include "../CCL.h"

class GameConfig;

namespace ActorCreator  
{
	template<typename... T>
	// Constructs an actor with the specified components <T...>, using the model from config. <T...> MUST contain GAME::Transform and DRAW::MeshCollection - E.A.
	entt::entity ConstructActor(entt::registry& registry, std::shared_ptr<const GameConfig> config, std::string configSection, GW::MATH::GMATRIXF transformMatrix = GW::MATH::GMATRIXF{ INFINITY });
};
#include "ActorCreator.inl"
#endif
