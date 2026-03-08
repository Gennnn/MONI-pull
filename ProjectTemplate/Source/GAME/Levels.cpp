#include "GameComponents.h"
#include "../CCL.h"
#include "../UTIL/Utilities.h"
#include "../DRAW/DrawComponents.h"
#include "../GAME/AudioComponents.h"
#include "ActorCreator.h"
#include <random>

namespace GAME {

	int trueLevelNum(int baseLevelNum, entt::registry& registry) {
		auto& lvls = registry.view<GAME::LevelManager>();
		for (auto ent : lvls) {
			auto& levelManager = registry.get<GAME::LevelManager>(ent);
			int lvlNum = 0;
			for (auto& lvl : levelManager.allLevels) {
				if (lvl.levelNum == baseLevelNum) {
					return lvlNum;
				}
				else {
					lvlNum++;
				}
			}
		}
	}

	void ChangeRooms(entt::registry& registry, entt::entity playerEntity, entt::entity doorEntity) {
		Transform& playerTransform = registry.get<Transform>(playerEntity);
		Transform& doorTransform = registry.get<Transform>(doorEntity);

		registry.patch<Transform>(playerEntity);
	}

	int FindNewRoom(GAME::Door& door, GAME::LevelManager& lvls, entt::registry& registry) {

		int lvlnum = 0;

		for (auto& lvl : lvls.allLevels) {
			if (lvl.levelNum == door.lvl) {
				return lvlnum;
			}
			else {
				lvlnum++;
			}

		}
		return 0;
	}

	void ZoomCamera(entt::registry& registry) {

		auto& LevelManagerView = registry.view<GAME::LevelManager>();

		bool changeX = false;
		bool changeY = false;
		bool changeZ = false;

		auto& camView = registry.view<DRAW::Camera>();
		for (auto ent : camView) {
			auto& camera = registry.get<DRAW::Camera>(ent);
			for (auto ent2 : LevelManagerView) {
				auto& levelManager = registry.get<GAME::LevelManager>(ent2);

				int currentlvl = trueLevelNum(levelManager.oldlvl, registry);
				int newlvl = trueLevelNum(levelManager.newlvl, registry);
				float distanceX = 0;
				float distanceY = 0;
				float distanceZ = 0;

				if (((camera.camMatrix.row4.x - levelManager.allLevels[newlvl].camPosition.row4.x >= 1) || (camera.camMatrix.row4.x - levelManager.allLevels[newlvl].camPosition.row4.x <= -1))) {
					//	std::cout << camera.camMatrix.row4.x << " " << levelManager.allLevels[levelManager.newlvl].camPosition.row4.x << std::endl;
						//std::cout << levelManager.allLevels[newlvl].camPosition.row4.x << " " << levelManager.allLevels[newlvl].camPosition.row4.y << levelManager.allLevels[newlvl].camPosition.row4.z <<std::endl;
					changeX = true;
					distanceX = levelManager.allLevels[newlvl].camPosition.row4.x - levelManager.allLevels[currentlvl].camPosition.row4.x;
				}
				if (((camera.camMatrix.row4.y - levelManager.allLevels[newlvl].camPosition.row4.y >= 1) || (camera.camMatrix.row4.y - levelManager.allLevels[newlvl].camPosition.row4.y <= -1))) {
					//	std::cout << camera.camMatrix.row4.y << " " << levelManager.allLevels[newlvl].camPosition.row4.y << std::endl;
					//	std::cout << camera.camMatrix.row4.y - levelManager.allLevels[newlvl].camPosition.row4.y >= 1;
					changeY = true;
					distanceY = levelManager.allLevels[newlvl].camPosition.row4.y - levelManager.allLevels[currentlvl].camPosition.row4.y;
				}
				if (((camera.camMatrix.row4.z - levelManager.allLevels[newlvl].camPosition.row4.z >= 1) || (camera.camMatrix.row4.z - levelManager.allLevels[newlvl].camPosition.row4.z <= -1))) {
					//	std::cout << camera.camMatrix.row4.z << " " << levelManager.allLevels[newlvl].camPosition.row4.z << std::endl;
					changeZ = true;
					distanceZ = levelManager.allLevels[newlvl].camPosition.row4.z - levelManager.allLevels[currentlvl].camPosition.row4.z;
				}

				std::shared_ptr<const GameConfig> config = registry.ctx().get<UTIL::Config>().gameConfig;
				float zoomSpeed = (*config).at("Camera").at("zoomSpeed").as<float>();
				UTIL::DeltaTime& deltaTime = registry.ctx().get<UTIL::DeltaTime>();
				camera.camMatrix.row4.x += (distanceX)*deltaTime.dtSec * zoomSpeed;
				camera.camMatrix.row4.y += (distanceY)*deltaTime.dtSec * zoomSpeed;
				camera.camMatrix.row4.z += (distanceZ)*deltaTime.dtSec * zoomSpeed;

				if ((!changeX && !changeY && !changeZ) || levelManager.cameraSafetyTimer >= 2/zoomSpeed) {
					camera.camMatrix.row4.x = levelManager.allLevels[newlvl].camPosition.row4.x;
					camera.camMatrix.row4.y = levelManager.allLevels[newlvl].camPosition.row4.y;
					camera.camMatrix.row4.z = levelManager.allLevels[newlvl].camPosition.row4.z;
					levelManager.ChangingCams = false;
					levelManager.cameraSafetyTimer = 0;
				}
			}
		}
	}

	void ClearLevelData(int lvlNum, entt::registry& registry, bool reset = false) {
		auto lvlView = registry.view<GAME::LevelManager>();

		for (auto ent : lvlView) {
			auto& levelManager = registry.get<GAME::LevelManager>(ent);
			if (reset) {
				levelManager.oldlvl = 0;
				levelManager.newlvl = 0;
			}
			else {
				levelManager.oldlvl = lvlNum;
			}
			for (auto& ent2 : levelManager.allLevels[trueLevelNum(lvlNum, registry)].destroyOnExit) {
				if (registry.all_of<DRAW::MeshCollection>(ent2))
					registry.emplace_or_replace<ToDestroy>(ent2);
			}


			levelManager.allLevels[trueLevelNum(lvlNum, registry)].destroyOnExit.clear();
		}
	}

	entt::entity FindNewDoor(GAME::Door& door, entt::registry& registry) {
		auto& doorView = registry.view<Door>();
		for (auto ent : doorView) {
			auto& newDoor = registry.get<GAME::Door>(ent);

			if (door.ID == newDoor.ID && door.lvl != newDoor.lvl) {

				return ent;

			}


		}


	}

	void Move_Player(entt::entity doorEnt, entt::entity player, entt::registry& registry) {

		auto& door = registry.get<GAME::Door>(doorEnt);
		auto& playerTransform = registry.get<Transform>(player);
		auto& doorTransform = registry.get<Transform>(doorEnt);

	
		switch (door.dir) {

		case 0: //Up

			playerTransform.matrix.row4.x = doorTransform.matrix.row4.x;
			playerTransform.matrix.row4.z = doorTransform.matrix.row4.z + 5;

			break;

		case 1: //Right
			playerTransform.matrix.row4.x = doorTransform.matrix.row4.x + 5;
			playerTransform.matrix.row4.z = doorTransform.matrix.row4.z;

			break;

		case 2: //Down
			playerTransform.matrix.row4.x = doorTransform.matrix.row4.x;
			playerTransform.matrix.row4.z = doorTransform.matrix.row4.z - 5;

			break;

		case 3: //Left
			playerTransform.matrix.row4.x = doorTransform.matrix.row4.x - 5;
			playerTransform.matrix.row4.z = doorTransform.matrix.row4.z;

			break;
		}

	}

	void ExitLevel(int lvlNum, entt::registry& registry) {
		auto lvlView = registry.view<GAME::LevelManager>();

		for (auto ent : lvlView) {
			auto& levelManager = registry.get<GAME::LevelManager>(ent);
			levelManager.oldlvl = lvlNum;
			for (auto& ent2 : levelManager.allLevels[trueLevelNum(lvlNum, registry)].destroyOnExit) {
				if (registry.all_of<DRAW::MeshCollection>(ent2))
					registry.emplace_or_replace<ToDestroy>(ent2);

				if (registry.all_of<Invuln, PassThroughWalls>(ent2)) {
					levelManager.allLevels[trueLevelNum(lvlNum, registry)].invulPos = registry.get<Transform>(ent2).matrix;
				}
			}


			levelManager.allLevels[trueLevelNum(lvlNum, registry)].destroyOnExit.clear();

			if (levelManager.allLevels[trueLevelNum(lvlNum, registry)].treasureCollected && lvlNum > 0) {
				auto coverView = registry.view<Cover>();
				for (auto& ent3 : coverView) {
					auto& coverEnt = registry.get<Cover>(ent3);
					if (coverEnt.lvl == lvlNum) {
						std::string name = "SubWorld";
						name += std::to_string(lvlNum);
						std::shared_ptr<const GameConfig> config = registry.ctx().get<UTIL::Config>().gameConfig;
						ActorCreator::ConstructActor<ReplicaCover>(registry, config, name);

					}
				}
				
				if (lvlNum > 0) {
					auto& view = registry.view<GAME::Door>();

					for (auto& ent : view) {
						auto& door = registry.get<GAME::Door>(ent);
						if (door.lvl == lvlNum) {
							auto tempDoor = FindNewDoor(door, registry);
							registry.erase<Active>(tempDoor);
						}

					}
				}
			}

		}
	}

	void Change_Camera(entt::registry& registry, int lvl) {
		auto& cam = registry.view<DRAW::Camera>();
		auto& lvls = registry.view<GAME::LevelManager>();




		for (auto ent : cam) {
			auto& camera = registry.get<DRAW::Camera>(ent);
			for (auto ent2 : lvls) {
				LevelManager& levelManager = registry.get<GAME::LevelManager>(ent2);

				if (levelManager.newlvl == levelManager.oldlvl) {

					int newRoom = trueLevelNum(lvl, registry);

					camera.camMatrix.row4.x = levelManager.allLevels[newRoom].camPosition.row4.x;
					camera.camMatrix.row4.y = levelManager.allLevels[newRoom].camPosition.row4.y;
					camera.camMatrix.row4.z = levelManager.allLevels[newRoom].camPosition.row4.z;
				}
				else {

					levelManager.ChangingCams = true;

				}





			}
		}
	}

	void Change_Rooms(entt::entity doorEnt, entt::entity player, entt::registry& registry) {

		auto gameManagerView = registry.view<GameManager>();
		bool hubCompleted = false;
		if (!gameManagerView.empty())
		{
			hubCompleted = CheckTreasureWinState(registry);
			
		}

		auto& cam = registry.view<DRAW::Camera>();
		auto& door = registry.get<GAME::Door>(doorEnt);
	
		auto& lvls = registry.view<GAME::LevelManager>();
		auto newDoorEnt = FindNewDoor(door, registry);
		LevelManager levelManager;

		int newRoom;
		GAME::Door newDoor = registry.get<GAME::Door>(newDoorEnt);

		newRoom = newDoor.lvl;

		
		

		
		int lvlID = door.lvl;
		ExitLevel(lvlID, registry);
		if (!hubCompleted) {
			OnLevelEntry(newRoom, registry);
			Move_Player(newDoorEnt, player, registry);
			
			Change_Camera(registry, newRoom);
		}
		
	}

	void InvulEnemySpawn(entt::registry& registry) {
		auto& lvls = registry.view<GAME::LevelManager>();
		std::shared_ptr<const GameConfig> config = registry.ctx().get<UTIL::Config>().gameConfig;

		for (auto ent : lvls) {
			auto& levelManager = registry.get<GAME::LevelManager>(ent);
			int lvl = trueLevelNum(levelManager.newlvl, registry);
			
			if (levelManager.allLevels[lvl].invulSpawned || levelManager.newlvl <= 0) {
				return;
			}
				std::string worldName = "SubWorld";
				worldName += std::to_string(levelManager.newlvl);
				float time = (*config).at(worldName).at("InvulEnemySpawnTimer").as<float>();

			if (levelManager.allLevels[lvl].timer >= time) {
				levelManager.allLevels[lvl].invulSpawned = true;
				

				GW::MATH::GMATRIXF spawnPoint = levelManager.allLevels[lvl].camPosition;
				spawnPoint.row4.y = 0;
				

				srand((unsigned int)std::time(nullptr));
				std::random_device rd;

				// Use Mersenne Twister engine for high-quality randomness
				std::mt19937 gen(rd());

				// Example 1: Uniform integer distribution between 1 and 100
				std::uniform_int_distribution<int> intDist(0, (*config).at(worldName).at("InvulEnemySpawnPositions").as<float>() - 1);
				
				int random = intDist(gen);
				std::string invulX = "InvulSpawnLocationX";
				std::string invulZ = "InvulSpawnLocationZ";

				switch (random) {
				case 0:
					invulX += std::to_string(1);
					invulZ += std::to_string(1);
					break;
				case 1:
					invulX += std::to_string(2);
					invulZ += std::to_string(2);
					break;
				case 2:
					invulX += std::to_string(3);
					invulZ += std::to_string(3);
					break;
				case 3:
					invulX += std::to_string(4);
					invulZ += std::to_string(4);
					break;
				
				}
				
				spawnPoint.row4.x = (*config).at(worldName).at(invulX).as<float>();
				spawnPoint.row4.z = (*config).at(worldName).at(invulZ).as<float>();

				entt::entity enemy = ActorCreator::ConstructActor<Enemy, Velocity, Collidable, Health, Invuln, PassThroughWalls>(registry, config, "InvulEnemy", spawnPoint);
				AUDIO::SetDrumsIntensity(registry, 17000, 0.5);
				AUDIO::playAudio(registry, "enforcerAppear", 1.0f, 0.0f, 0.97f, 1.03f);
				levelManager.allLevels[lvl].destroyOnExit.push_back(enemy);

			}
		}
	}


	void SubLevelSpawn(int lvl, entt::registry& registry) {
		auto& lvls = registry.view<GAME::LevelManager>();
		std::string worldName = "SubWorld" + std::to_string(lvl);
		std::shared_ptr<const GameConfig> config = registry.ctx().get<UTIL::Config>().gameConfig;
		auto& playerView = registry.view<GAME::Player>();
		for (auto ent : playerView) {
			auto& player = registry.get<GAME::Player>(ent);
			player.hub = false;
		}

		GW::MATH::GMATRIXF origin; // Center of the level

		for (auto ent : lvls) {
			auto& levelManager = registry.get<GAME::LevelManager>(ent);
			int truelevel = trueLevelNum(lvl, registry);
			origin = levelManager.allLevels[truelevel].camPosition;
			origin.row4.y = 0;
			origin.row4.z += 5;

			//Anything inside here will only happen the FIRST time the room is entered
			if (levelManager.allLevels[truelevel].firstEntry) {
				levelManager.allLevels[truelevel].firstEntry = false;
				Change_Camera(registry, lvl);

			}

			if (levelManager.allLevels[truelevel].invulSpawned) {
				entt::entity enemy = ActorCreator::ConstructActor<Enemy, Velocity, Collidable, Health, Invuln, PassThroughWalls>(registry, config, "InvulEnemy", levelManager.allLevels[truelevel].invulPos);
				levelManager.allLevels[truelevel].destroyOnExit.push_back(enemy);
			}

			//Spawn enemies here
			GW::MATH::GMATRIXF spawnPoint = origin;
			int enemyCount = (*config).at(worldName).at("EnemySpawnCount").as<int>();
			std::string enemyTag;
			for (int i = 0; i < enemyCount; i++) {
				enemyTag = "EnemySpawn" + std::to_string(i + 1);
				std::string enemyName = (*config).at(worldName).at(enemyTag).as<std::string>();
				std::string enemyX = "EnemySpawnLocationX" + std::to_string(i+1);
				std::string enemyZ = "EnemySpawnLocationZ" + std::to_string(i+1);
				spawnPoint.row4.x = (*config).at(worldName).at(enemyX).as<float>();
				spawnPoint.row4.z = (*config).at(worldName).at(enemyZ).as<float>();
				entt::entity enemy = ActorCreator::ConstructActor<GAME::Enemy, Velocity, Collidable, GAME::Health>(registry, config, enemyName, spawnPoint);
				auto& enemyTransform = registry.get<Transform>(enemy);
				enemyTransform.matrix.row4.y = origin.row4.y;
				
				levelManager.allLevels[truelevel].destroyOnExit.push_back(enemy);
			}

			int breakableWallCount = (*config).at(worldName).at("BreakableWallCount").as<int>();
			
			for (int i = 0; i < breakableWallCount; i++) {
				std::string wallX = "BreakableWallLocationX" + std::to_string(i + 1);
				std::string wallY = "BreakableWallLocationY" + std::to_string(i + 1);
				std::string wallZ = "BreakableWallLocationZ" + std::to_string(i + 1);
				spawnPoint.row4.x = (*config).at(worldName).at(wallX).as<float>();
				spawnPoint.row4.y = (*config).at(worldName).at(wallY).as<float>();
				spawnPoint.row4.z = (*config).at(worldName).at(wallZ).as<float>();
				entt::entity wall = ActorCreator::ConstructActor<BreakableWall, Collidable, Obstacle>(registry, config, "BreakableWall", spawnPoint);

				levelManager.allLevels[truelevel].destroyOnExit.push_back(wall);
			}
			spawnPoint.row4.x = (*config).at(worldName).at("TreasureSpawnLocationX").as<float>();
			spawnPoint.row4.y = origin.row4.y;
			spawnPoint.row4.z = (*config).at(worldName).at("TreasureSpawnLocationZ").as<float>();
			std::string treasure = (*config).at(worldName).at("Treasure").as<std::string>();
			entt::entity treasureEnt = ActorCreator::ConstructActor<GAME::Treasure, GAME::Collidable>(registry, config, treasure, spawnPoint);
			levelManager.allLevels[truelevel].destroyOnExit.push_back(treasureEnt);
		}

	}



	void HubLevelSpawn(int lvl, entt::registry& registry) {
		
		auto& lvls = registry.view<GAME::LevelManager>();
		std::string worldName = "HubWorld" + std::to_string((lvl * -1)+1);
		std::shared_ptr<const GameConfig> config = registry.ctx().get<UTIL::Config>().gameConfig;
		auto& playerView = registry.view<GAME::Player>();
		for (auto ent : playerView) {
			auto& player = registry.get<GAME::Player>(ent);
			player.hub = true;
		}

		GW::MATH::GMATRIXF origin; // Center of the level

		for (auto ent : lvls) {
			auto& levelManager = registry.get<GAME::LevelManager>(ent);
			int truelevel = trueLevelNum(lvl, registry);
			origin = levelManager.allLevels[truelevel].camPosition;
			origin.row4.y = 0;
			origin.row4.z += 5;
			GW::MATH::GMATRIXF spawnPoint = origin;
			//Anything inside here will only happen the FIRST time the room is entered
			if (levelManager.allLevels[truelevel].firstEntry) {
				levelManager.allLevels[truelevel].firstEntry = false;
				Change_Camera(registry, lvl);
				std::string playerX = "PlayerSpawnX";
				std::string playerZ = "PlayerSpawnZ";

				auto& playerView = registry.view<GAME::Player>();
				for (auto ent : playerView) {
					auto& player = registry.get<GAME::Player>(ent);
					player.hub = true;
					Transform& playerTransform = registry.get<Transform>(ent);
					playerTransform.matrix.row4.x = (*config).at(worldName).at(playerX).as<float>();
					playerTransform.matrix.row4.z = (*config).at(worldName).at(playerZ).as<float>();
				}

				

			}
			//Anything inside here will happen only when the level is loaded for the first time, does not count softResets
			if (levelManager.allLevels[truelevel].firstInstance) {
				//Life Functionality
				int LifeCount = (*config).at(worldName).at("LifeCount").as<int>();

				for (int i = 0; i < LifeCount; i++) {
					std::string LifeX = "LifeX" + std::to_string(i + 1);
					std::string LifeZ = "LifeZ" + std::to_string(i + 1);
					spawnPoint.row4.x = (*config).at(worldName).at(LifeX).as<float>();
					spawnPoint.row4.z = (*config).at(worldName).at(LifeZ).as<float>();
					entt::entity life = ActorCreator::ConstructActor<GAME::Collidable, GAME::LifePickup>(registry, config, "Life", spawnPoint);
					auto& lifeTransform = registry.get<Transform>(life);
					lifeTransform.matrix.row4.y = origin.row4.y;
					//levelManager.allLevels[trueLevelNum(lvl, registry)].destroyOnExit.push_back(life);
				}
				levelManager.allLevels[truelevel].firstInstance = false;
			}
			//Spawn enemies here
			int enemyCount = (*config).at(worldName).at("EnemySpawnCount").as<int>();
			std::string enemyTag;
			for (int i = 0; i < enemyCount; i++) {
				enemyTag = "EnemySpawn" + std::to_string(i + 1);
				std::string enemyName = (*config).at(worldName).at(enemyTag).as<std::string>();
				std::string enemyX = "EnemySpawnLocationX" + std::to_string(i + 1);
				std::string enemyZ = "EnemySpawnLocationZ" + std::to_string(i + 1);
				spawnPoint.row4.x = (*config).at(worldName).at(enemyX).as<float>();
				spawnPoint.row4.z = (*config).at(worldName).at(enemyZ).as<float>();
				entt::entity enemy = ActorCreator::ConstructActor<GAME::Enemy, Velocity, Collidable, GAME::Health>(registry, config, enemyName, spawnPoint);
				auto& enemyTransform = registry.get<Transform>(enemy);
				enemyTransform.matrix.row4.y = origin.row4.y;
				if (enemyName == "InvulEnemy") {
					registry.emplace<GAME::Invuln>(enemy);
				}

				levelManager.allLevels[trueLevelNum(lvl, registry)].destroyOnExit.push_back(enemy);
			}

			

		}
	}

	void OnLevelEntry(int levelnum, entt::registry& registry) {
		auto& lvlman = registry.view<GAME::LevelManager>();
		auto& playerView = registry.view<GAME::Player>();
		bool hub = true;
		for (auto ent : lvlman) {
			auto& levelManager = registry.get<GAME::LevelManager>(ent);
			levelManager.newlvl = levelnum;
			for (auto ent : playerView) {
				auto& player = registry.get<GAME::Player>(ent);
				if (levelnum > 0) {
					hub = false;
					player.hub = false;
				}
				else {
					hub = true;
					player.hub = hub;
					
				}
				if (hub || levelManager.allLevels[trueLevelNum(levelnum, registry)].invulSpawned) {
					AUDIO::SetDrumsIntensity(registry, 17000.0f, 0.5);
				}
				else {
					AUDIO::SetDrumsIntensity(registry, 1500.0f, 0.5);
				}
			}
			
		}

		switch (hub) {
		case true: //Hub
			HubLevelSpawn(levelnum, registry);
			break;
		case false:
			SubLevelSpawn(levelnum, registry);			
			break;

		}

	}

	void UpdateLevels(entt::registry& registry) {
		UTIL::DeltaTime& deltaTime = registry.ctx().get<UTIL::DeltaTime>();

		auto& lvlman = registry.view<GAME::LevelManager>();

		for (auto ent : lvlman) {
			auto& levelManager = registry.get<GAME::LevelManager>(ent);
			if (levelManager.ChangingCams) {
				ZoomCamera(registry);
				levelManager.cameraSafetyTimer += deltaTime.dtSec;
			}
			auto& gameManager = registry.view<GameManager>();
			if (!registry.any_of<Pause>(*gameManager.begin())) {
				levelManager.allLevels[trueLevelNum(levelManager.newlvl, registry)].timer += deltaTime.dtSec;
			}
			InvulEnemySpawn(registry);
		}
	}

	CONNECT_COMPONENT_LOGIC() {
		registry.on_update<LevelManager>().connect<UpdateLevels>();
	}
}