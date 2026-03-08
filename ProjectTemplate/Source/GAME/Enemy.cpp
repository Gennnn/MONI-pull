#include "../CCL.h"
#include "../UTIL/Utilities.h"
#include "ActorCreator.h"
#include "GameComponents.h"
#include <random>

namespace GAME {

	static void constructEnemy(entt::registry& registry, entt::entity entity) {
		Enemy* enemy = registry.try_get<Enemy>(entity);
		ConfigSection* configSection = registry.try_get<ConfigSection>(entity);
		if (configSection == nullptr) return;
		std::shared_ptr<const GameConfig> config = registry.ctx().get<UTIL::Config>().gameConfig;
		enemy->speed = (*config).at(configSection->configSection).at("speed").as<float>();
		enemy->sightRadius = (*config).at(configSection->configSection).at("sightRadius").as<float>();
		enemy->cooldown = 0;
		enemy->baseCooldown = (*config).at(configSection->configSection).at("roamCooldown").as<float>();
		enemy->roamRadius = (*config).at(configSection->configSection).at("roamRadius").as<float>();
	}
	/// <summary>
	/// Handle the overall enemy behavior and generation logic
	/// GenerateEnemies - Initialize the enemy spawner, spawn enemies based on the config file settings
	/// create enemy movement and player chase behaviors
	/// create enemy attack behaviors
	/// </summary>
	void GenerateEnemies(entt::registry& registry, std::shared_ptr<const GameConfig> config, int rAmount, int iAmount, std::vector<GW::MATH::GVECTORF> spawnLocales) {
		// Generate the specified amount of enemies based on the amount given
		std::vector<entt::entity> enemies;
		for (int i = 0; i < rAmount; i++) {
			entt::entity enemy = ActorCreator::ConstructActor<GAME::Enemy, GAME::Velocity, GAME::Collidable, GAME::Health>(registry, config, "Enemy1");
		}
		for (int i = 0; i < iAmount; i++) {
			entt::entity invulEnemy = ActorCreator::ConstructActor<GAME::Enemy, GAME::Velocity, GAME::Collidable, GAME::Health, GAME::Invuln>(registry, config, "InvulEnemy");
		}
		int spawnIndex = 0;
		for (auto& enemy : enemies) {
			// spawn enemies at the given location in the spawnLocales vector
			if (spawnLocales.size() != 0 && spawnIndex < spawnLocales.size()) {
				GW::MATH::GMATRIXF enemyMatrix;
				GW::MATH::GMatrix::IdentityF(enemyMatrix);
				GW::MATH::GMatrix::TranslateGlobalF(enemyMatrix, spawnLocales[spawnIndex], enemyMatrix);
				registry.get<GAME::Transform>(enemy).matrix = enemyMatrix;
				spawnIndex++;
			}
		}
	}


	void ChasePlayer(entt::registry& registry, entt::entity entity) {
		//Grab enemy transform and player transform
		Transform& enemTransform = registry.get<Transform>(entity);
		auto& playerTransform = registry.view<GAME::Player>();
		float radius = registry.get<Enemy>(entity).sightRadius;
		for (auto ent : playerTransform)
		{
			Transform& playerTrans = registry.get<Transform>(ent);
			if (((enemTransform.matrix.row4.x - playerTrans.matrix.row4.x >= -radius && enemTransform.matrix.row4.x - playerTrans.matrix.row4.x <= radius) && (enemTransform.matrix.row4.z - playerTrans.matrix.row4.z >= -radius && enemTransform.matrix.row4.z - playerTrans.matrix.row4.z <= radius)) || registry.all_of<PassThroughWalls>(entity)) {
				//set the target position for the enemy to move towards

				registry.get<Enemy>(entity).posX = playerTrans.matrix.row4.x;
				registry.get<Enemy>(entity).posZ = playerTrans.matrix.row4.z;

				registry.emplace_or_replace<PlayerInRange>(entity);
			}
			else {
				registry.remove<PlayerInRange>(entity);
				//PositionSelection(registry, entity);
			}
		}
	}

	void PositionSelection(entt::registry& registry, entt::entity entity) {
		//grab the enemy transform
		Transform& enemTransform = registry.get<Transform>(entity);
		Enemy& enemy = registry.get<Enemy>(entity);

		srand((unsigned int)std::time(nullptr));
		std::random_device rd;

		
		static std::mt19937 gen(rd());

		
		std::uniform_int_distribution<int> intDist(0, 32607);

		int xMin = enemTransform.matrix.row4.x - enemy.roamRadius;
		int xMax = enemTransform.matrix.row4.x + enemy.roamRadius;
		int selectedPointX = xMin + (intDist(gen) % (xMax - xMin + 1));
		int zMin = enemTransform.matrix.row4.z - enemy.roamRadius;
		int zMax = enemTransform.matrix.row4.z + enemy.roamRadius;
		int selectedPointZ = zMin + (intDist(gen) % (zMax - zMin + 1));

		//set the target position for the enemy to move towards
		enemy.posX = selectedPointX;
		enemy.posZ = selectedPointZ;
		//updateRotation(registry, entity);
	}
	void updateRotation(entt::registry& registry, entt::entity entity) {

		//grab the enemy transform
		
		Transform& enemTransform = registry.get<Transform>(entity);
		Velocity& enemVelocity = registry.get<Velocity>(entity);
		float velocityX = enemVelocity.currentVelocity.x;
		float velocityZ = enemVelocity.currentVelocity.z;
		Enemy& enemy = registry.get<Enemy>(entity);
		float angle = atan2f(velocityZ, velocityX); //Temporary Fix, I know the problem but we'll handle it later
		if (enemy.rotCooldown > 0) {
			enemy.rotCooldown -= registry.ctx().get<UTIL::DeltaTime>().dtSec;
			return;
		}
		if ((enemTransform.matrix.row4.x - enemy.posX >= 0.1f || enemTransform.matrix.row4.x - enemy.posX <= -0.1f) && (enemTransform.matrix.row4.z - enemy.posZ >= 0.1f || enemTransform.matrix.row4.z - enemy.posZ <= -0.1f)
			|| registry.all_of<Reflected>(entity)) {
			angle += 3.14159f / 2; // Adjust angle to face the correct direction
			angle *= -1.0f; // Invert angle to match coordinate system
			//std::cout << "Angle: " << angle << std::endl;
			float transX = enemTransform.matrix.row4.x;
			float transZ = enemTransform.matrix.row4.z;
			float transY = enemTransform.matrix.row4.y;
			GW::MATH::GMatrix::IdentityF(enemTransform.matrix);
			GW::MATH::GVECTORF translation;
			translation.x = transX;
			translation.y = transY;
			translation.z = transZ;

			GW::MATH::GMatrix::TranslateGlobalF(enemTransform.matrix, translation, enemTransform.matrix);
			GW::MATH::GMatrix::RotateYLocalF(enemTransform.matrix, angle, enemTransform.matrix);
		}
	}
	void decreaseReflectionCooldown(entt::registry& registry, entt::entity entity) {
		Enemy& enemy = registry.get<Enemy>(entity);
		Reflected* eneRef = registry.try_get<Reflected>(entity);
		if (eneRef == nullptr) return;
		if (eneRef->cooldown > 0) {
			eneRef->cooldown -= registry.ctx().get<UTIL::DeltaTime>().dtSec;
		}
		else {
			eneRef->cooldown = eneRef->baseCooldown;
			eneRef->collisions = 0;
			registry.remove<PassThroughWalls>(entity);
		}
	}
	void HandleMovement(entt::registry& registry, entt::entity entity) {
		//Grab enemy transform, velocity, and enemy components
		Transform& enemTransform = registry.get<Transform>(entity);
		Velocity& enemVelocity = registry.get<Velocity>(entity);
		Enemy& enemy = registry.get<Enemy>(entity);
		if ((!registry.all_of<GAME::Reflected>(entity)) || registry.all_of<PassThroughWalls>(entity)) {
			GW::MATH::GVECTORF direction = { enemy.posX - enemTransform.matrix.row4.x, 0.0f, enemy.posZ - enemTransform.matrix.row4.z };
			GW::MATH::GVector::NormalizeF(direction, direction);
			float speed = enemy.speed;
			float& vSpeed = registry.get<Velocity>(entity).speed;
			//std::shared_ptr<const GameConfig> config = registry.ctx().get<UTIL::Config>().gameConfig;
			if (registry.all_of<Invuln>(entity)) {
				auto& lvlView = registry.view<LevelManager>();
				for (auto lvlman : lvlView) {
					LevelManager& manager = registry.get<LevelManager>(lvlman);
					if (manager.newlvl <= 0)
					speed *= ((manager.allLevels[trueLevelNum(manager.newlvl, registry)].timer / 10) + 1);
					vSpeed = speed;
				}
			}
			GW::MATH::GVector::ScaleF(direction, speed, direction); // Adjust the speed of the enemy
			enemVelocity.currentVelocity = direction;
		}
		if (!registry.all_of<EnemyDepression>(entity) || registry.all_of<PassThroughWalls>(entity)) {
			ChasePlayer(registry, entity);
		}
		else {
			EnemyDepression& depression = registry.get<EnemyDepression>(entity);
			depression.cooldown -= registry.ctx().get<UTIL::DeltaTime>().dtSec;
			if (depression.cooldown <= 0) {
				registry.remove<EnemyDepression>(entity);
			}
		}

		if (enemy.cooldown > 0) {
			enemy.cooldown -= registry.ctx().get<UTIL::DeltaTime>().dtSec;
			if (enemy.cooldown <= 0) {
				enemy.cooldown = 0;
			}
		}
		else {
			enemy.cooldown = enemy.baseCooldown;
			registry.remove<Reflected>(entity);
			if (!(registry.all_of<PlayerInRange>(entity))) {
				PositionSelection(registry, entity);
				
			}
			
		}
	}

	void Update_Enemy(entt::registry& registry, entt::entity entity) {
		// Update enemy behavior each frame
		HandleMovement(registry, entity);
		updateRotation(registry, entity);
		decreaseReflectionCooldown(registry, entity);
		
	}

	CONNECT_COMPONENT_LOGIC() {
		registry.on_update<Enemy>().connect<Update_Enemy>();
		registry.on_construct<Enemy>().connect<constructEnemy>();
	}
}