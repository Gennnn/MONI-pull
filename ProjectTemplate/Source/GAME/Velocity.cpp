#include "GameComponents.h"
#include "../CCL.h"
#include "../UTIL/Utilities.h"


namespace GAME {
	/// <summary>
	/// Update behavior for velocity component
	/// </summary>
	void Update_Velocity(entt::registry& registry, entt::entity entity) {
		Transform* transform = registry.try_get<Transform>(entity);
		if (transform == nullptr) return;

		Velocity& velocity = registry.get<Velocity>(entity);
		UTIL::DeltaTime& deltaTime = registry.ctx().get<UTIL::DeltaTime>();
		std::shared_ptr<const GameConfig> config = registry.ctx().get<UTIL::Config>().gameConfig;
		GW::MATH::GVECTORF movementPerUpdate;
		GW::MATH::GVector::ScaleF(velocity.currentVelocity, deltaTime.dtSec, movementPerUpdate);
		GW::MATH::GVector::AddVectorF(transform->matrix.row4, movementPerUpdate, transform->matrix.row4);
		auto prevTran = registry.try_get<PreviousTransform>(entity);
		if (prevTran != nullptr) {
			if (prevTran->timer > 0) {
				prevTran->timer -= deltaTime.dtSec;
				return;
			}
			prevTran->timer = prevTran->baseTimer;
			float Prev5x = prevTran->matrix5.row4.x;
			float Prev5z = prevTran->matrix5.row4.z;
			float Prev4x = prevTran->matrix4.row4.x;
			float Prev4z = prevTran->matrix4.row4.z;
			float Prev3x = prevTran->matrix3.row4.x;
			float Prev3z = prevTran->matrix3.row4.z;
			float Prev2x = prevTran->matrix2.row4.x;
			float Prev2z = prevTran->matrix2.row4.z;
			float Prev1x = prevTran->matrix1.row4.x;
			float Prev1z = prevTran->matrix1.row4.z;
			float currx = transform->matrix.row4.x;
			float currz = transform->matrix.row4.z;
			float offset = 3.7f;

			if ((Prev5x - offset > Prev4x  || Prev5x + offset < Prev4x) || (Prev5z - offset > Prev4z || Prev5z + offset < Prev4z)) {
				prevTran->matrix5 = prevTran->matrix4;
			}
			if ((Prev4x - offset > Prev3x || Prev4x + offset < Prev3x) || (Prev4z - offset > Prev3z || Prev4z + offset < Prev3z)) {
				prevTran->matrix4 = prevTran->matrix3;
			}
			if ((Prev3x - offset > Prev2x || Prev3x + offset < Prev2x) || (Prev3z - offset > Prev2z || Prev3z + offset < Prev2z)) {
				prevTran->matrix3 = prevTran->matrix2;
			}
			if ((Prev2x - offset > Prev1x || Prev2x + offset < Prev1x) || (Prev2z - offset > Prev1z || Prev2z + offset < Prev1z)) {
				prevTran->matrix2 = prevTran->matrix1;
			}
			if ((Prev1x - offset > currx || Prev1x + offset < currx) || (Prev1z - offset > currz || Prev1z + offset < currz)) {
				prevTran->matrix1 = transform->matrix;
			}
		}
		else {
			PreviousTransform previousTransform;
			previousTransform.matrix1 = transform->matrix;
			previousTransform.matrix2 = transform->matrix;
			previousTransform.matrix3 = transform->matrix;
			previousTransform.matrix4 = transform->matrix;
			previousTransform.matrix5 = transform->matrix;
			registry.emplace_or_replace<PreviousTransform>(entity, previousTransform);
		}
	}

	/// <summary>
	/// Automatic setup for speed triggered upon velocity component construction
	/// </summary>
	void Construct_Velocity(entt::registry& registry, entt::entity entity) {
		Velocity* velocity = registry.try_get<Velocity>(entity);
		ConfigSection* configSection = registry.try_get <ConfigSection>(entity);
		if (configSection == nullptr) return;

		std::shared_ptr<const GameConfig> config = registry.ctx().get<UTIL::Config>().gameConfig;
		float speed = (*config).at(configSection->configSection).at("speed").as<float>();
		velocity->speed = speed;
		
	}

	/// <summary>
	/// Set the speed component of an object's velocity
	/// </summary>
	/// <param name="registry">EnTT's registry</param>
	/// <param name="entity">The entity with a velocity component attached</param>
	/// <param name="newSpeed">The new speed for the velocity component</param>
	void SetSpeed(entt::registry& registry, entt::entity entity, float newSpeed) {
		Velocity* velocity = registry.try_get<Velocity>(entity);
		if (velocity == nullptr) return;
		velocity->speed = newSpeed;
	}

	/// <summary>
	/// Set the direction component of an object's velocity. 
	/// </summary>
	/// <param name="registry">EnTT's registry</param>
	/// <param name="entity">The entity with a velocity component attached</param>
	/// <param name="direction">The new direction for the velocity component</param>
	void SetDirection(entt::registry& registry, entt::entity entity, GW::MATH::GVECTORF direction) {
		Velocity* velocity = registry.try_get<Velocity>(entity);
		if (velocity == nullptr) return;
		float magnitude = sqrtf(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
		GW::MATH::GVECTORF& currentVelocity = velocity->currentVelocity;
		currentVelocity = magnitude == 0.0f ? GW::MATH::GVECTORF{ 0,0,0 } : GW::MATH::GVECTORF{ direction.x / magnitude,direction.y / magnitude,direction.z / magnitude };
		currentVelocity = GW::MATH::GVECTORF{ currentVelocity.x * velocity->speed,currentVelocity.y * velocity->speed,currentVelocity.z * velocity->speed, 0 };
	}

	CONNECT_COMPONENT_LOGIC() {
		registry.on_update<Velocity>().connect<Update_Velocity>();
		registry.on_construct<Velocity>().connect<Construct_Velocity>();
	}
}
