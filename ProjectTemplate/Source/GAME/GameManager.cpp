#include "GameComponents.h"
#include "../DRAW/DrawComponents.h"
#include "../CCL.h"
#include "ActorCreator.h"
#include "../UTIL/Utilities.h"
#include "AudioComponents.h"
#include "SaveScore.h"
#include <algorithm>
#include "Timer.h"

namespace GAME {

	// Raymond's addition: Only print the saved scores list once per app run
	static bool g_printedSavedScores = false;

	// Raymond's addition: Track run time for the current game
	static Timer g_runTimer;

	static constexpr double runTimeLimit = 20.0;

	// Raymond's addition: Print all saved scores from file
	static void PrintSavedScoresList()
	{
		auto scores = SaveScore::Load();

		std::cout << "----- Top 10 Scores -----" << std::endl;

		if (scores.empty())
		{
			std::cout << "(no scores saved yet)" << std::endl;
		}
		else
		{
			for (auto& s : scores)
			{
				std::cout << s.name << "    - " << s.score << std::endl;
			}
		}

		std::cout << "------------------------" << std::endl;
	}


	/// <summary>
	/// Handle enemy-wall collision behavior (enemy reflects off of wall)
	/// </summary>
	/// <param name="registry">EnTT's registry</param>
	/// <param name="entityA">Entity A involved in the collision event</param>
	/// <param name="entityB">Entity B involved in the collision event</param>
	/// <param name="colliderA">Entity A's collider</param>
	/// <param name="colliderB">Entity B's collider</param>
	void HandleEnemyWallCollision(entt::registry& registry, entt::entity entityA, entt::entity entityB, GW::MATH::GOBBF& colliderA, GW::MATH::GOBBF& colliderB, const GW::MATH::GMATRIXF& prevPlayerMatrix) {
		auto& enemyEntity = registry.all_of<Enemy>(entityA) ? entityA : entityB;
		auto& obstacleEntity = registry.all_of<Obstacle>(entityA) ? entityA : entityB;
		auto& enemyCollider = enemyEntity == entityA ? colliderA : colliderB;
		auto& obstacleCollider = obstacleEntity == entityA ? colliderA : colliderB;

		auto& enem = registry.get<Enemy>(enemyEntity);

		registry.get<Transform>(enemyEntity).matrix = prevPlayerMatrix;

		registry.patch<Transform>(enemyEntity);

	}

	/// <summary>
	/// Handle bullet-wall collision behavior (bullet is destroyed)
	/// </summary>
	/// <param name="registry">EnTT's registry</param>
	/// <param name="entityA">Entity A involved in the collision event</param>
	/// <param name="entityB">Entity B involved in the collision event</param>
	void HandleBulletWallCollision(entt::registry& registry, entt::entity entityA, entt::entity entityB) {
		auto& bulletEntity = registry.all_of<Bullet>(entityA) ? entityA : entityB;
		registry.emplace_or_replace<ToDestroy>(bulletEntity);
	}
	/// <summary>
	/// Handle bullet-wall collision behavior (bullet is destroyed)
	/// </summary>
	/// <param name="registry">EnTT's registry</param>
	/// <param name="entityA">Entity A involved in the collision event</param>
	/// <param name="entityB">Entity B involved in the collision event</param>
	void HandlePlayerLifeCollision(entt::registry& registry, entt::entity entityA, entt::entity entityB) {
		auto& LifeEntity = registry.all_of<LifePickup>(entityA) ? entityA : entityB;
		auto& playerEntity = registry.all_of<Player>(entityA) ? entityA : entityB;
		auto& gameManagerView = registry.view<GameManager>();
		auto& config = registry.ctx().get<UTIL::Config>().gameConfig;
		if (!gameManagerView.empty()) {
			entt::entity gameManager = *gameManagerView.begin();
			auto& lives = registry.get<Lives>(gameManager);
			int lifeCap = (*config).at("Player").at("lifeCap").as<int>();
			if (lives.value < lifeCap) {
				lives.value++;
			}
			AUDIO::playAudio(registry, "CollectSound", 1.0f, 0.0f, 1.5f, 1.7f);
		}
		registry.destroy(LifeEntity);
	}

	/// <summary>
	/// Handle bullet-enemy collision behavior (bullet is destroyed, enemy health is decremented)
	/// </summary>
	/// <param name="registry">EnTT's registry</param>
	/// <param name="entityA">Entity A involved in the collision event</param>
	/// <param name="entityB">Entity B involved in the collision event</param>
	void HandleBulletEnemyCollision(entt::registry& registry, entt::entity entityA, entt::entity entityB, std::shared_ptr<const GameConfig> config) {
		auto& bulletEntity = registry.all_of<Bullet>(entityA) ? entityA : entityB;
		auto& enemyEntity = registry.all_of<Enemy>(entityA) ? entityA : entityB;
		registry.emplace_or_replace<ToDestroy>(bulletEntity);
		if (ModifyHealth(registry, enemyEntity, 1) <= 0) {

			// Raymond's addition: Award score for defeating an enemy (+100)
			auto gmView = registry.view<GameManager, Score>();
			if (gmView.begin() != gmView.end())
			{
				entt::entity gm = *gmView.begin();
				registry.patch<Score>(gm, [](Score& score) {
					score.value += 100;
					});
				// Raymond's addition: Print updated score after enemy defeat.
				//std::cout << "Enemy defeated! +100 | Score: " << registry.get<Score>(gm).value << std::endl;
				AUDIO::playAudio(registry, "newEnemyDeath", 1.0f, 0.0f, 0.9f, 1.1f);
			}
			auto deadEnem = ActorCreator::ConstructActor<DeadEnemyModel, Lifetime>(registry, config, "DeadBot1", registry.get<Transform>(enemyEntity).matrix);
			auto& enemDespawnTime = registry.get<Lifetime>(deadEnem);
			enemDespawnTime.seconds = 2.0f;
			auto& lvlman = registry.view<LevelManager>();
			for (auto& ent : lvlman) {
				auto& man = registry.get<LevelManager>(ent);
				man.allLevels[trueLevelNum(man.newlvl, registry)].destroyOnExit.push_back(deadEnem);
			}
		}
		else if (registry.all_of<GAME::Invuln>(enemyEntity)) {
			AUDIO::playAudio(registry, "enforcerHit", 1.0f, 0.0f, 0.9f, 1.1f);
		}
	}


	/// <summary>
	/// Handle player-enemy collision behavior (player health is decremented and invulnerability is applied)
	/// </summary>
	/// <param name="registry">EnTT's registry</param>
	/// <param name="entityA">Entity A involved in the collision event</param>
	/// <param name="entityB">Entity B involved in the collision event</param>
	void HandlePlayerEnemyCollision(entt::registry& registry, entt::entity entityA, entt::entity entityB, std::shared_ptr<const GameConfig> config) {
		auto& playerEntity = registry.all_of<Player>(entityA) ? entityA : entityB;
		if (registry.try_get<Invulnerability>(playerEntity) != nullptr) return;
		if (ModifyHealth(registry, playerEntity, 1) <= 0) {
			ActorCreator::ConstructActor<DeadPlayerModel>(registry, config, "DeadPlayer", registry.get<Transform>(playerEntity).matrix);
			registry.emplace_or_replace<ToDestroy>(playerEntity);


		}
		else {
			ApplyInvulnerability(registry, playerEntity, "Player");
		}

	}

	void BreakWall(entt::registry& registry, entt::entity entityA, entt::entity entityB) {
		auto& wallEntity = registry.all_of<BreakableWall>(entityA) ? entityA : entityB;
		registry.emplace_or_replace<ToDestroy>(wallEntity);
	}

	// Raymond's addition: Reverts player movement if they collide with a wall (Player Collision)
	void HandlePlayerWallCollision(entt::registry& registry, entt::entity entityA, entt::entity entityB, const GW::MATH::GMATRIXF& prevPlayerMatrix)
	{
		// Identify which entity is the player
		entt::entity playerEntity = registry.all_of<Player>(entityA) ? entityA : entityB;

		// Revert the player's transform to what it was before movement
		registry.get<Transform>(playerEntity).matrix = prevPlayerMatrix;

		registry.patch<Transform>(playerEntity);
	}

	/// <summary>
	/// Check to see if either of our game over conditions have been met
	/// </summary>
	/// <param name="registry">EnTT's registry</param>
	/// <param name="gameManagerEntity">The entity containing the Game Manager component</param>
	void CheckGameOverState(entt::registry& registry, entt::entity gameManagerEntity) {
		auto playerAlive = registry.view<Player>();
		if (playerAlive.empty()) {
			auto& lives = registry.get<Lives>(gameManagerEntity);
			//std::cout << lives.value << std::endl;
			if (lives.value == 1) {
				lives.value = lives.value - 1;
				registry.emplace_or_replace<GameOver>(gameManagerEntity);
				//std::cout << "You lose, game over" << std::endl;
				FreezeDelay freezeDelay;
				freezeDelay.cooldown = 5.0f;
				AUDIO::stopAllPlayingAudio(registry);
				registry.emplace_or_replace<FreezeDelay>(gameManagerEntity, freezeDelay);
				AUDIO::playAudio(registry, "plrDeath", 1.0f);
				return;
			}
			else {
				lives.value = lives.value - 1;
				registry.emplace_or_replace<SoftGameOver>(gameManagerEntity);

				//std::cout << "You died! Lives remaining: " << lives.value << std::endl;
				FreezeDelay freezeDelay;
				freezeDelay.cooldown = 5.0f;
				AUDIO::stopAllPlayingAudio(registry);
				registry.emplace_or_replace<FreezeDelay>(gameManagerEntity, freezeDelay);
				AUDIO::playAudio(registry, "plrDeath", 1.0f);
				return;
			}

		}

		/*
		auto enemiesKilled = registry.view<Enemy>();
		if (enemiesKilled.empty()) {
			registry.emplace_or_replace<GameOver>(gameManagerEntity);
			std::cout << "You win, good job!" << std::endl;
			return;
		}
		*/
	}

	// Raymond's addition: Ends the game when all treasure has been collected.
	bool CheckTreasureWinState(entt::registry& registry)
	{
		auto& levelManView = registry.view<LevelManager>();
		bool win = true;
		std::string level = "HubWorld";
		std::shared_ptr<const GameConfig> config = registry.ctx().get<UTIL::Config>().gameConfig;
		int levelsCompleted = 0;
		for (auto ent : levelManView) {
			auto& levelManager = registry.get<LevelManager>(ent);
			level += std::to_string(levelManager.hubsCompleted + 1);
			for (auto level : levelManager.allLevels) {
				if (!level.treasureCollected && level.levelNum > 0) {
					win = false;

				}
				else if (level.levelNum > 0) {
					levelsCompleted++;
				}
			}

			if (levelsCompleted == config->at(level).at("TreasureCount").as<int>() && levelManager.hubsCompleted == 0) {
				ExitLevel(levelManager.hubsCompleted * -1, registry);
				levelManager.hubsCompleted++;
				levelManager.newlvl = (levelManager.hubsCompleted * -1);
				levelManager.oldlvl = (levelManager.hubsCompleted * -1);
				Change_Camera(registry, (levelManager.hubsCompleted * -1));
				OnLevelEntry((levelManager.hubsCompleted * -1), registry);
				auto& view = registry.view<Player>();
				for (auto playerEnt : view) {
					auto& trans = registry.get <Transform>(playerEnt);
					level = "HubWorld" + std::to_string(levelManager.hubsCompleted + 1);
					trans.matrix.row4.x = config->at(level).at("PlayerSpawnX").as<float>();
					trans.matrix.row4.z = config->at(level).at("PlayerSpawnZ").as<float>();
				}
				return true;
			}
		}





		if (win)
		{
			auto gameManagerView = registry.view<GameManager>();

			auto& view = registry.view<LevelManager>();
			for (auto& ent : view) {
				auto& manager = registry.get<LevelManager>(ent);
				manager.newlvl = 0;
				manager.oldlvl = 0;
			}
			Change_Camera(registry, 0);

			FreezeDelay freezeDelay;
			freezeDelay.cooldown = 7.0f;
			AUDIO::stopAllPlayingAudio(registry);
			registry.emplace_or_replace<FreezeDelay>(*gameManagerView.begin(), freezeDelay);
			registry.emplace_or_replace<GameOver>(*gameManagerView.begin(), GameOver{ true });
			//std::cout << "All treasure collected, you win!" << std::endl;

			Score* score = registry.try_get<Score>(*gameManagerView.begin());
			Timer timer = registry.get<Timer>(*gameManagerView.begin());

			score->value += (timer.GetSeconds() * 10);
			AUDIO::playAudio(registry, "winSFX", 1.0f);
			DRAW::SetUIState(registry, DRAW::COMPLETED);
		}
		return false;
	}
	//Basically everything we had from main, ported into one function!
	void pressStart(entt::registry& registry, std::shared_ptr<const GameConfig> config, entt::entity gameManager) {
		entt::entity player = ActorCreator::ConstructActor<GAME::Player, GAME::Collidable, GAME::Health>(registry, config, "Player");
		registry.emplace_or_replace<GAME::Lives>(gameManager);

		// Raymond's addition: Reset timer at the start of a new run
		double timeLimit = (*config).at("Level1").at("timeLimit").as<double>();
		registry.emplace_or_replace<Timer>(gameManager).Reset(timeLimit);

		DRAW::SetUIState(registry, DRAW::IN_GAME);

		GAME::Lives& playerLives = registry.get<GAME::Lives>(gameManager);
		GAME::ConfigSection playerConfig = registry.get<GAME::ConfigSection>(player);

		playerLives.value = (*config).at(playerConfig.configSection).at("lives").as<int>();

		HubLevelSpawn(0, registry);
		std::vector<GW::MATH::GVECTORF> locales;
		//GAME::GenerateEnemies(registry, config, 2, 1, locales);


		// Raymond's addition: Ask for player name once per app run (stored on GameManager)
		//auto& pname = registry.get<PlayerName>(gameManager);
		//if (pname.value.empty())
		//{
		//	std::cout << "Enter your name: ";
		//	std::getline(std::cin, pname.value);

		//	// If getline reads an empty string due to buffered newline, try once more
		//	if (pname.value.empty())
		//		std::getline(std::cin, pname.value);

		//	if (pname.value.empty())
		//		pname.value = "Player";
		//}
		registry.get<Score>(gameManager).value = 0;
		//Raymond's addition: Spawn 3 treasures at fixed locations.

		//const GW::MATH::GVECTORF treasurePositions[1] = {
		//	//{ -7.3f,  1.52f,  17.1f, 1.0f },
		//	{  5.0f,  1.52f, -10.0f, 1.0f }
		//};
		auto& doorViews = registry.view<GAME::Door>();
		for (auto doorEnt = doorViews.begin(); doorEnt != doorViews.end(); doorEnt++) {
			registry.emplace_or_replace<GAME::Active>(*doorEnt);
		}

		//AUDIO::playAudio(registry, AUDIO::AudioVariant::GAME_STARTUP);

		AUDIO::playAudio(registry, "StartSound", 1.0f);

		FreezeDelay freezeDelay;
		freezeDelay.cooldown = 4.0f;
		registry.emplace_or_replace<FreezeDelay>(gameManager, freezeDelay);
		Change_Camera(registry, 0);
		registry.emplace_or_replace<PlayBGMusicNextFrame>(gameManager);
	}
	//Basically, the exact OPPOSITE of pressStart, to clean up the level and reset everything for a new game.
	void GameOverCleanup(entt::registry& registry, std::shared_ptr<const GameConfig> config, entt::entity gameManager) {
		auto players = registry.view<Player>();
		for (auto playerEntity = players.begin(); playerEntity != players.end(); playerEntity++) {
			registry.destroy(*playerEntity);
		}
		auto enemies = registry.view<Enemy>();
		for (auto enemyEntity = enemies.begin(); enemyEntity != enemies.end(); enemyEntity++) {
			registry.destroy(*enemyEntity);
		}
		auto treasures = registry.view<Treasure>();
		for (auto treasureEntity = treasures.begin(); treasureEntity != treasures.end(); treasureEntity++) {
			registry.destroy(*treasureEntity);
		}
		auto bullets = registry.view<Bullet>();
		for (auto bulletEntity = bullets.begin(); bulletEntity != bullets.end(); bulletEntity++) {
			registry.emplace_or_replace<ToDestroy>(*bulletEntity);
		}
		auto Covers = registry.view<ReplicaCover>();
		for (auto coverEntity = Covers.begin(); coverEntity != Covers.end(); coverEntity++) {
			registry.destroy(*coverEntity);
		}
		auto deadPlayers = registry.view<DeadPlayerModel>();
		for (auto deadPlayerEntity = deadPlayers.begin(); deadPlayerEntity != deadPlayers.end(); deadPlayerEntity++) {
			registry.destroy(*deadPlayerEntity);
		}
		auto& view = registry.view<LevelManager>();
		for (auto& ent : view) {
			auto& manager = registry.get<LevelManager>(ent);
			manager.newlvl = 0;
			manager.oldlvl = 0;
			manager.hubsCompleted = 0;
			manager.levelAndTreasureId.clear();
			for (auto& lvl : manager.allLevels) {
				lvl.firstEntry = true;
				lvl.timer = 0;
				lvl.treasureCollected = false;
				lvl.invulSpawned = false;
				lvl.firstInstance = true;
			}

		}

	}
	//A softer gameOverCleanup!
	void softReset(entt::registry& registry, std::shared_ptr<const GameConfig> config, entt::entity gameManager) {
		auto players = registry.view<Player>();
		for (auto playerEntity = players.begin(); playerEntity != players.end(); playerEntity++) {
			registry.destroy(*playerEntity);
		}
		auto enemies = registry.view<Enemy>();
		for (auto enemyEntity = enemies.begin(); enemyEntity != enemies.end(); enemyEntity++) {
			registry.destroy(*enemyEntity);
		}

		auto bullets = registry.view<Bullet>();
		for (auto bulletEntity = bullets.begin(); bulletEntity != bullets.end(); bulletEntity++) {
			registry.emplace_or_replace<ToDestroy>(*bulletEntity);
		}
		auto deadPlayers = registry.view<DeadPlayerModel>();
		for (auto deadPlayerEntity = deadPlayers.begin(); deadPlayerEntity != deadPlayers.end(); deadPlayerEntity++) {
			registry.destroy(*deadPlayerEntity);
		}
		auto& view = registry.view<LevelManager>();
		for (auto& ent : view) {
			auto& manager = registry.get<LevelManager>(ent);
			manager.allLevels[trueLevelNum(manager.newlvl, registry)].treasureCollected = false;
			manager.levelAndTreasureId.erase(trueLevelNum(manager.newlvl, registry));
			ClearLevelData(manager.newlvl, registry, true);
			manager.allLevels[trueLevelNum(0, registry)].firstEntry = true;
			manager.newlvl = manager.hubsCompleted * -1;
			manager.oldlvl = manager.hubsCompleted * -1;
			for (auto& lvl : manager.allLevels) {
				lvl.firstEntry = true;
				lvl.timer = 0;
				lvl.invulSpawned = false;
			}
		}
	}
	//A softer press start!
	void respawn(entt::registry& registry, entt::entity gameManager) {
		std::shared_ptr<const GameConfig> config = registry.ctx().get<UTIL::Config>().gameConfig;

		entt::entity player = ActorCreator::ConstructActor<GAME::Player, GAME::Collidable, GAME::Health>(registry, config, "Player");

		auto& view = registry.view<LevelManager>();
		for (auto ent : view) {
			auto& manager = registry.get<LevelManager>(ent);
			manager.newlvl = manager.hubsCompleted * -1;
			manager.oldlvl = manager.hubsCompleted * -1;
			for (auto lvl : manager.allLevels) {
				lvl.firstEntry = true;
			}
			HubLevelSpawn(manager.hubsCompleted * -1, registry);
			Change_Camera(registry, manager.hubsCompleted * -1);
		}


		std::vector<GW::MATH::GVECTORF> locales;
		//GAME::GenerateEnemies(registry, config, 2, 1, locales);
		AUDIO::playAudio(registry, "StartSound", 1.0f);
		FreezeDelay freezeDelay;
		freezeDelay.cooldown = 4.0f;
		registry.emplace_or_replace<FreezeDelay>(gameManager, freezeDelay);
		registry.emplace_or_replace<PlayBGMusicNextFrame>(gameManager);
		registry.remove<WaitingForReady>(gameManager);
	}

	void Update_GameManager(entt::registry& registry, entt::entity entity) {
		UTIL::Input& input = registry.ctx().get<UTIL::Input>();

		//UTIL::printFPS(registry);

		auto instances = registry.group<Transform, DRAW::MeshCollection>();
		for (auto instancedEntity = instances.begin(); instancedEntity != instances.end(); instancedEntity++) {
			DRAW::MeshCollection& meshCollection = registry.get<DRAW::MeshCollection>(*instancedEntity);
			for (int j = 0; j < meshCollection.entities.size(); j++) {
				registry.get<DRAW::GPUInstance>(meshCollection.entities[j]).transform = registry.get<Transform>(*instancedEntity).matrix;
			}
		}
		auto toDestroy = registry.view<ToDestroy>();
		for (auto entityToDestroy = toDestroy.begin(); entityToDestroy != toDestroy.end(); entityToDestroy++) {
			registry.destroy(*entityToDestroy);
		}
		//FreezeDelay State
		if (registry.all_of<FreezeDelay>(entity)) {
			auto deltaTime = registry.ctx().get<UTIL::DeltaTime>().dtSec;
			auto& freezeDelay = registry.get<FreezeDelay>(entity);
			freezeDelay.cooldown -= deltaTime;
			if (freezeDelay.cooldown <= 0) {
				registry.remove<FreezeDelay>(entity);
			}
			return;
		}
		if (registry.all_of<PlayBGMusicNextFrame>(entity)) {
			//AUDIO::playAudio(registry, AUDIO::AudioVariant::BGMUSIC);
			AUDIO::SetActiveSoundscape(registry, "gameplay");
			AUDIO::StartMusic(registry);
			AUDIO::SetDrumsIntensity(registry, 17000, 0.5);
			registry.remove<PlayBGMusicNextFrame>(entity);
		}
		//Pre-Start State
		if (!registry.all_of<Started>(entity)) {

			// Raymond's addition: Show saved scores list once per application run
			if (!g_printedSavedScores)
			{
				//PrintSavedScoresList();
				g_printedSavedScores = true;
			}


			return;
		}
		//Waiting for respawn State
		if (registry.all_of<WaitingForReady>(entity)) {
			/*float start;
			input.immediateInput.GetState(G_KEY_SPACE, start);
			if (start > 0.0f) {
				respawn(registry, entity);
				registry.remove<WaitingForReady>(entity);
				DRAW::SetUIState(registry, DRAW::IN_GAME);
			}*/
			return;
		}
		//Soft Game Over state
		if (registry.all_of<SoftGameOver>(entity)) {
			//Do game over stuff here, then we remove the component to reset the game.
			softReset(registry, registry.ctx().get<UTIL::Config>().gameConfig, entity);
			registry.remove<SoftGameOver>(entity);
			registry.emplace_or_replace<WaitingForReady>(entity);
			AUDIO::stopAllPlayingAudio(registry);
			DRAW::SetUIState(registry, DRAW::DEATH);
			//std::cout << "Press SPACE to respawn!" << std::endl;
			return;
		}
		//Game Over State
		if (registry.all_of<GameOver>(entity)) {
			//Do game over stuff here, then we remove the component to reset the game.
			GameOverCleanup(registry, registry.ctx().get<UTIL::Config>().gameConfig, entity);

			bool won = registry.get<GameOver>(entity).won;

			registry.remove<GameOver>(entity);
			registry.remove<Started>(entity);
			if (won) {
				DRAW::SetUIState(registry, DRAW::WIN);
			}
			else {
				if (registry.any_of<TimerExpired>(entity)) {
					DRAW::SetUIState(registry, DRAW::TIME_UP);
				}
				else {
					DRAW::SetUIState(registry, DRAW::DEATH);
				}
				
				AUDIO::playAudio(registry, "GameOver", 1.0f);
			}
			// Raymond's addition: Update high score (persists until app closes)
			auto& score = registry.get<Score>(entity);
			auto& high = registry.get<HighScore>(entity);
			auto& pname = registry.get<PlayerName>(entity);

			//int runSeconds = static_cast<int>(g_runTimer.GetSeconds());
			//std::cout << "Time: " << Timer::FormatHHMMSS(runSeconds) << std::endl;

			// Raymond's addition: Save player's score to file (persists after app closes)
			//SaveScore::Save(pname.value, score.value);

			if (score.value > high.value)
			{
				high.value = score.value;
				high.name = pname.value;
				//std::cout << "NEW HIGH SCORE! " << high.name << " - " << high.value << std::endl;
			}
			else
			{
				//std::cout << "High Score: " << high.name << " - " << high.value << std::endl;
			}

			//AUDIO::stopAllPlayingAudio(registry);

			//std::cout << "Press SPACE to restart!" << std::endl;
			return;
		}
		//Afterwards, we know the game is in an active state, so we can do everything else.


		CheckGameOverState(registry, entity);

		auto& lvlManagerView = registry.view<LevelManager>();
		for (auto ent : lvlManagerView) {
			auto& manager = registry.get<LevelManager>(ent);
			registry.patch<LevelManager>(ent);

		}

		entt::basic_group player = registry.group(entt::get<Player>);
		for (int i = 0; i < player.size(); i++) {
			registry.patch<Player>(player[i]);
		}



		if (registry.all_of<Pause>(entity)) return;

		// Raymond's addition: Update run timer during active gameplay

		Timer* timer = registry.try_get<Timer>(entity);
		if (timer != nullptr && !registry.any_of<TimerExpired>(entity)) {
			timer->Update(registry.ctx().get<UTIL::DeltaTime>().dtSec);
			if (timer->IsExpired()) {
				registry.emplace_or_replace<TimerExpired>(entity);
			}
		}
		

		entt::basic_group velocity = registry.group(entt::get<Velocity>);
		for (int i = 0; i < velocity.size(); i++) {
			registry.patch<Velocity>(velocity[i]);
		}

		auto& collisions = registry.view<Transform, DRAW::MeshCollection, Collidable>();

		for (auto entityA = collisions.begin(); entityA != collisions.end(); entityA++) {
			GW::MATH::GMATRIXF& transformMatrixA = registry.get<Transform>(*entityA).matrix;
			GW::MATH::GOBBF colliderA = registry.get<DRAW::MeshCollection>(*entityA).collider;

			GW::MATH::GVECTORF scaleVectorA;
			GW::MATH::GMatrix::GetScaleF(transformMatrixA, scaleVectorA);

			colliderA.extent.x *= scaleVectorA.x;
			colliderA.extent.y *= scaleVectorA.y;
			colliderA.extent.z *= scaleVectorA.z;
			colliderA.center.w = 1.0f;
			GW::MATH::GMatrix::VectorXMatrixF(transformMatrixA, colliderA.center, colliderA.center);

			GW::MATH::GQUATERNIONF quaternionA;
			GW::MATH::GQuaternion::SetByMatrixF(transformMatrixA, quaternionA);
			GW::MATH::GQuaternion::MultiplyQuaternionF(colliderA.rotation, quaternionA, colliderA.rotation);

			auto entityB = entityA;
			for (entityB++; entityB != collisions.end(); entityB++) {
				GW::MATH::GMATRIXF& transformMatrixB = registry.get<Transform>(*entityB).matrix;
				GW::MATH::GOBBF colliderB = registry.get<DRAW::MeshCollection>(*entityB).collider;

				GW::MATH::GVECTORF scaleVectorB;
				GW::MATH::GMatrix::GetScaleF(transformMatrixB, scaleVectorB);

				colliderB.extent.x *= scaleVectorB.x;
				colliderB.extent.y *= scaleVectorB.y;
				colliderB.extent.z *= scaleVectorB.z;
				colliderB.center.w = 1.0f;
				GW::MATH::GMatrix::VectorXMatrixF(transformMatrixB, colliderB.center, colliderB.center);

				GW::MATH::GQUATERNIONF quaternionB;
				GW::MATH::GQuaternion::SetByMatrixF(transformMatrixB, quaternionB);
				GW::MATH::GQuaternion::MultiplyQuaternionF(colliderB.rotation, quaternionB, colliderB.rotation);

				GW::MATH::GCollision::GCollisionCheck collisionResult;
				GW::MATH::GCollision::TestOBBToOBBF(colliderA, colliderB, collisionResult);
				if (collisionResult == GW::MATH::GCollision::GCollisionCheck::COLLISION) {

					if ((registry.all_of<Bullet>(*entityA) && registry.all_of<Obstacle>(*entityB)) || (registry.all_of<Bullet>(*entityB) && registry.all_of<Obstacle>(*entityA))) {

						HandleBulletWallCollision(registry, *entityA, *entityB);
					}

					if ((registry.all_of<Enemy>(*entityA) && registry.all_of<Obstacle>(*entityB)) || (registry.all_of<Enemy>(*entityB) && registry.all_of<Obstacle>(*entityA))) {
						entt::entity EnemyEntity = registry.all_of<Enemy>(*entityA) ? *entityA : *entityB;
						auto* prev = registry.try_get<PreviousTransform>(EnemyEntity);
						if (prev != nullptr)
						{

							GW::MATH::GMATRIXF prevPlayerMatrix = prev->matrix5;
							if (!registry.all_of<PassThroughWalls>(*entityA) && !registry.all_of<PassThroughWalls>(*entityB))
							HandleEnemyWallCollision(registry, *entityA, *entityB, colliderA, colliderB, prevPlayerMatrix);
						}
						
					}

					if ((registry.all_of<Enemy>(*entityA) && registry.all_of<Bullet>(*entityB)) || (registry.all_of<Enemy>(*entityB) && registry.all_of<Bullet>(*entityA))) {
						HandleBulletEnemyCollision(registry, *entityA, *entityB, registry.ctx().get<UTIL::Config>().gameConfig);
					}

					if ((registry.all_of<Enemy>(*entityA) && registry.all_of<Player>(*entityB)) || (registry.all_of<Enemy>(*entityB) && registry.all_of<Player>(*entityA))) {
						HandlePlayerEnemyCollision(registry, *entityA, *entityB, registry.ctx().get<UTIL::Config>().gameConfig);
					}

					if ((registry.all_of<BreakableWall>(*entityA) && registry.all_of<Bullet>(*entityB)) || (registry.all_of<BreakableWall>(*entityB) && registry.all_of<Bullet>(*entityA))) {
						BreakWall(registry, *entityA, *entityB);
					}

					// Raymond's addition: Handles player�wall collision (prevents phasing through walls)
					if ((registry.all_of<Player>(*entityA) && registry.all_of<Obstacle>(*entityB)) ||
						(registry.all_of<Player>(*entityB) && registry.all_of<Obstacle>(*entityA)))
					{
						entt::entity playerEntity = registry.all_of<Player>(*entityA) ? *entityA : *entityB;

						// Pull the previous transform that saved right before movement
						auto* prev = registry.try_get<PreviousTransform>(playerEntity);
						if (prev != nullptr)
						{
							GW::MATH::GMATRIXF prevPlayerMatrix = prev->matrix5;
							HandlePlayerWallCollision(registry, *entityA, *entityB, prevPlayerMatrix);
						}
					}
					if ((registry.all_of<Player>(*entityA) && registry.all_of<LifePickup>(*entityB)) || (registry.all_of<Player>(*entityB) && registry.all_of<LifePickup>(*entityA))) {
						HandlePlayerLifeCollision(registry, *entityA, *entityB);
					}

					// Raymond's addition: Handles playertreasure collision and removes the treasure on pickup.
					if ((registry.all_of<Treasure>(*entityA) && registry.all_of<Player>(*entityB)) || (registry.all_of<Treasure>(*entityB) && registry.all_of<Player>(*entityA)))
					{
						entt::entity treasureEntity =
							registry.all_of<Treasure>(*entityA) ? *entityA : *entityB;

						registry.emplace_or_replace<ToDestroy>(treasureEntity);

						// Raymond's addition: Award score for collecting treasure (+250)
						registry.patch<Score>(entity, [](Score& score) {score.value += 250; });
						AUDIO::playAudio(registry, "CollectSound", 1.0f, 0.0f, 0.9f, 1.2f);
						// Raymond's addition: Print updated score after treasure pickup
						//std::cout << "Treasure collected! +250 | Score: " << registry.get<Score>(entity).value << std::endl;

						Treasure treasure = registry.get<Treasure>(treasureEntity);

						// Allows the level cover to spawn once the room is left
						auto& lvlManagerView = registry.view<LevelManager>();
						for (auto& ent : lvlManagerView) {
							auto& manager = registry.get<LevelManager>(ent);
							manager.allLevels[trueLevelNum(manager.newlvl, registry)].treasureCollected = true;

							manager.levelAndTreasureId.insert({ trueLevelNum(manager.newlvl, registry), treasure.id });
						}


					}

					if (registry.all_of<Door, Active>(*entityA) && registry.all_of<Player>(*entityB)) {
						//Door entering SFX
						//AUDIO::playAudio(registry, AUDIO::AudioVariant::DOOR_ENTER);
						AUDIO::playAudio(registry, "DoorEnter", 1.0f, 0.0f, 0.95f, 1.05f);

						Change_Rooms(*entityA, *entityB, registry);
					}
					if (registry.all_of<Door, Active>(*entityB) && registry.all_of<Player>(*entityA)) {
						//Door entering SFX
						AUDIO::playAudio(registry, "DoorEnter", 1.0f, 0.0f, 0.95f, 1.05f);

						Change_Rooms(*entityB, *entityA, registry);
					}

				}
			}

			if (registry.any_of<TimerExpired>(entity) && !registry.any_of<GameOver, SoftGameOver>(entity)) {
				Lives lives = registry.get<Lives>(entity);
				lives.value = 0;
				registry.emplace_or_replace<GameOver>(entity, GameOver{ false });
				FreezeDelay freezeDelay;
				freezeDelay.cooldown = 5.0f;
				AUDIO::stopAllPlayingAudio(registry);
				registry.emplace_or_replace<FreezeDelay>(entity, freezeDelay);
				AUDIO::playAudio(registry, "plrDeath", 1.0f);
			}
		}


		auto health = registry.view<Health>();
		for (auto instance = health.begin(); instance != health.end(); instance++) {
			if (registry.get<Health>(*instance).amount <= 0) {
				registry.emplace_or_replace<ToDestroy>(*instance);
			}
		}

		auto enemy = registry.view<Enemy>();
		for (auto instance = enemy.begin(); instance != enemy.end(); instance++) {
			registry.patch<Enemy>(*instance);
		}

		auto view = registry.view<Lifetime>();
		for (auto& e : view) {
			auto& life = view.get<Lifetime>(e);
			life.seconds -= registry.ctx().get<UTIL::DeltaTime>().dtSec;
			if (life.seconds <= 0) {
				if (registry.valid(e))	{
					registry.destroy(e);
				}
			}
		}

	}



	CONNECT_COMPONENT_LOGIC() {
		registry.on_update<GameManager>().connect<Update_GameManager>();
	}

}
