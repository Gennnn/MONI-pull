#include "GameComponents.h"
#include "../CCL.h"
#include "../UTIL/Utilities.h"
#include "ActorCreator.h"
#include "AudioComponents.h"
#include "../DRAW/DrawComponents.h"

namespace GAME {

	void CheckPauseInput(entt::registry& registry, UTIL::Input& input) {
		float pause;
		input.immediateInput.GetState(G_KEY_P, pause);
		if (pause > 0.0f) pauseFunction(registry);
	}

	bool pauseFunction(entt::registry& registry) {
		auto gameManagerView = registry.view<GameManager>();
		entt::entity gameManager = *gameManagerView.begin();
		bool hasPause = registry.all_of<Pause>(gameManager);
		if (!(registry.all_of<PauseCooldown>(gameManager))) {
			if (hasPause) {
				DRAW::SetUIState(registry, DRAW::UIState::IN_GAME, false);
				registry.remove<Pause>(gameManager);
				PauseCooldown newCooldown = { 0.2f };
				registry.emplace_or_replace<PauseCooldown>(gameManager, newCooldown);
				DRAW::SetUISelectorActive(registry, false);
				return false;
				
			}
			else {
				registry.emplace_or_replace<Pause>(gameManager);
				DRAW::SetUIState(registry, DRAW::UIState::PAUSE);
				PauseCooldown newCooldown = { 0.2f };
				registry.emplace_or_replace<PauseCooldown>(gameManager, newCooldown);
				DRAW::SetUISelectorActive(registry, true);
				return true;
			}
		}
		DRAW::SetUISelectorActive(registry, hasPause);
		return hasPause;
	}
	/// <summary>
	/// Handle the "update loop" movement behavior for the player.
	/// </summary>
	/// <initial commit>
	/// <param name="registry">EnTT's registry</param>
	/// <param name="entity">The entity that the player component is attached to</param>
	/// <param name="deltaTime">A reference to the delta time component emplaced on the registry's context</param>
	/// <param name="input">A reference to the input component emplaced on the registry's context</param>
	/// <param name="config">A reference to the config emplaced on the registry's context</param>
	
	void DoMovement(entt::registry& registry, entt::entity entity, UTIL::DeltaTime& deltaTime, UTIL::Input& input, std::shared_ptr<const GameConfig> config) {
		
		//Pull movespeed from the config file for the player
		float moveSpeed = (*config).at("Player").at("speed").as<float>();
		Player& player = registry.get<Player>(entity);
		if (player.hub) {
			moveSpeed *= (*config).at("Player").at("hubSpeedMult").as<float>();
		}

		//Get the separate input directions for movement
		float up, down, left, right;
		input.immediateInput.GetState(G_KEY_W, up);
		input.immediateInput.GetState(G_KEY_S, down);
		input.immediateInput.GetState(G_KEY_D, right);
		input.immediateInput.GetState(G_KEY_A, left);

		//Combine inputs, normalize, and multiply by movespeed and deltaTime to get the amount to move per-frame
		GW::MATH::GVECTORF inputVec = { right - left , up - down  };
		float magnitude = inputVec.x == 0 && inputVec.y == 0 ? 0.0f : sqrtf(inputVec.x * inputVec.x + inputVec.y * inputVec.y);
		inputVec = magnitude == 0.0f ? GW::MATH::GVECTORF{ 0,0 } : GW::MATH::GVECTORF{ inputVec.x / magnitude, inputVec.y / magnitude };
		GW::MATH::GVECTORF movementVector = { inputVec.x * moveSpeed * deltaTime.dtSec, 0,  inputVec.y * moveSpeed * deltaTime.dtSec };

		

		//Modify the player's transform based on the calculated per-frame movement
		Transform& transform = registry.get<Transform>(entity);
		PreviousTransform* prevTran = registry.try_get<PreviousTransform>(entity);
		if (prevTran == nullptr) {
			registry.emplace<PreviousTransform>(entity, PreviousTransform{ transform.matrix, transform.matrix, transform.matrix, transform.matrix, transform.matrix });
		}
		else {
			
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
			float currx = transform.matrix.row4.x;
			float currz = transform.matrix.row4.z;
			float offset = 3.0f;

			if ((Prev5x - offset > Prev4x || Prev5x + offset < Prev4x) || (Prev5z - offset > Prev4z || Prev5z + offset < Prev4z)) {
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
				prevTran->matrix1 = transform.matrix;
			}
		}
		
		
		GW::MATH::GVector::AddVectorF(transform.matrix.row4, movementVector, transform.matrix.row4);

		if (movementVector.x == 0 && movementVector.z == 0) return;

		player.direction = { movementVector.x, 0, movementVector.z, 0 };

		float transX = transform.matrix.row4.x;
		float transZ = transform.matrix.row4.z;
		float transY = transform.matrix.row4.y;
		GW::MATH::GMatrix::IdentityF(transform.matrix);
		GW::MATH::GVECTORF translation;
		translation.x = transX;
		translation.y = transY;
		translation.z = transZ;

		GW::MATH::GMatrix::TranslateGlobalF(transform.matrix, translation, transform.matrix);
		
		float angle = atan2f(-movementVector.z, movementVector.x);
		angle += 3.14159f / 2;
		//angle *= -1.0f;

		GW::MATH::GMatrix::RotateYLocalF(transform.matrix, angle, transform.matrix);
		
		
	}

	/// <summary>
	/// Handle the "update loop" shooting behavior for the player.
	/// </summary>
	/// <param name="registry">EnTT's registry</param>
	/// <param name="entity">The entity that the player component is attached to</param>
	/// <param name="deltaTime">A reference to the delta time component emplaced on the registry's context</param>
	/// <param name="input">A reference to the input component emplaced on the registry's context</param>
	/// <param name="config">A reference to the config emplaced on the registry's context</param>

	void DoShooting(entt::registry& registry, entt::entity entity, UTIL::DeltaTime& deltaTime, UTIL::Input& input, std::shared_ptr<const GameConfig> config) {
		
		//Check if there's already a firing component attached to the player
		Firing* firing = registry.try_get<Firing>(entity);
		auto& player = registry.get<Player>(entity);

		if (firing != nullptr) {
			//If there is a firing component, subtract the time since last frame from the cooldown
			firing->cooldown -= deltaTime.dtSec;
			//If the cooldown is still greater than 0, firing is still on cooldown and we can't fire
			if (firing->cooldown > 0) return;
			//If cooldown less than or equal to 0, remove the firing component and continue since the player can fire again
			registry.remove<Firing>(entity);
		}

		//Get the separate input directions for firing
		float space;
		input.immediateInput.GetState(G_KEY_SPACE, space);
		
		if (space == 0) return;


		//Combine inputs, if there is no input we return since the player isn't currently attempting to fire (or doesn't have a valid shoot direction)
		GW::MATH::GVECTORF inputVec = player.direction; 
		
		
		

		//Normalize the direction
	//	float magnitude = sqrtf(inputVec.x * inputVec.x + inputVec.z * inputVec.z);
	//	inputVec = GW::MATH::GVECTORF{ inputVec.x / magnitude, 0, inputVec.z / magnitude, 0 };

		//Get the player's transform to inform the bullet's initial transform
		Transform transform = registry.get<Transform>(entity);
		
		//Construct Bullet
		entt::entity bulletInstance = ActorCreator::ConstructActor<Bullet, Velocity, Collidable>(registry, config, "Bullet", transform.matrix);
		
		//Set initial direction and velocity
		SetDirection(registry, bulletInstance, inputVec);

		//Emplace a firing component on the player to put them on firing cooldown
		Firing newFiring;
		newFiring.cooldown = (*config).at("Player").at("firerate").as<float>();
		registry.emplace<Firing>(entity,  std::move(newFiring));
		
		//play shoot sound effect
		AUDIO::playAudio(registry, "new shoot sound", 0.8f, 0.0f, 0.95f, 1.05f);
		
	}

	/// <summary>
	/// Handle the "update loop" invulnerability behavior for the player
	/// </summary>
	/// <param name="registry">EnTT's registry</param>
	/// <param name="entity">The entity that the player component is attached to</param>
	/// <param name="deltaTime">A reference to the delta time component emplaced on the registry's context</param>
	void DoInvulnerability(entt::registry& registry, entt::entity entity, UTIL::DeltaTime& deltaTime) {
		Invulnerability* invul = registry.try_get<Invulnerability>(entity);
		if (invul != nullptr) {
			invul->cooldown -= deltaTime.dtSec;
			if (invul->cooldown > 0) return;
			registry.remove<Invulnerability>(entity);
		}
	}

	/// <summary>
	/// Attach invulnerability to the player for the duration of time specified under "invulnPeriod" in the supplied config section
	/// </summary>
	/// <param name="registry">EnTT's registry</param>
	/// <param name="entity">The entity that the player component is attached to</param>
	/// <param name="configSection">The config section containing invulnPeriod to pull the duration from</param>
	void ApplyInvulnerability(entt::registry& registry, entt::entity entity, std::string configSection) {
		std::shared_ptr<const GameConfig> config = registry.ctx().get<UTIL::Config>().gameConfig;
		registry.emplace_or_replace<Invulnerability>(entity).cooldown = (*config).at(configSection).at("invulnPeriod").as<float>();
	}

	/// <summary>
	/// Handle the player's behavior on every game tick (or "update loop")
	/// </summary>
	/// <param name="registry">EnTT's registry</param>
	/// <param name="entity">The entity that the player component is attached to</param>
	void Update_Player(entt::registry& registry, entt::entity entity) {
		UTIL::DeltaTime& deltaTime = registry.ctx().get<UTIL::DeltaTime>();
		UTIL::Input& input = registry.ctx().get<UTIL::Input>();
		std::shared_ptr<const GameConfig> config = registry.ctx().get<UTIL::Config>().gameConfig;

		CheckPauseInput(registry, input);
		auto gameManagerView = registry.view<GameManager>();
		entt::entity gameManager = *gameManagerView.begin();
		if (registry.all_of<PauseCooldown>(gameManager)) {
			GAME::PauseCooldown& pauseCooldown = registry.get<PauseCooldown>(gameManager);
			pauseCooldown.cooldown -= deltaTime.dtSec;
			if (pauseCooldown.cooldown <= 0) {
				registry.remove<PauseCooldown>(gameManager);
				
			}
		}
		if (registry.all_of<Pause>(gameManager)) {
			return;
		}

		// Raymond's addition: Save the player's transform (Player Collision).
		

		DoMovement(registry, entity, deltaTime, input, config);
		DoShooting(registry, entity, deltaTime, input, config);
		DoInvulnerability(registry, entity, deltaTime);
	}

	CONNECT_COMPONENT_LOGIC() {
		registry.on_update<Player>().connect<Update_Player>();
	}
}