#ifndef GAME_COMPONENTS_H_
#define GAME_COMPONENTS_H_

class GameConfig;

namespace GAME
{
	///*** Tags ***///

	struct Obstacle {};
	struct Bullet {};
	struct Collidable{};
	struct BreakableWall {};
	struct ToDestroy{};
	
	struct Pause {};
	struct Started {};
	struct Invuln {};
	struct SoftGameOver {};
	struct WaitingForReady {};
	struct PlayBGMusicNextFrame {};
	struct Active {};
	struct ReplicaCover {};
	struct DeadPlayerModel {};
	struct DeadEnemyModel {};
	
	struct PlayerInRange {};
	struct PassThroughWalls {};
	struct LifePickup {};

	struct TimerExpired {};

	///*** Components ***///
	
	struct Reflected {
		int collisions = 0;
		float cooldown = 0.0f;
		float baseCooldown = 0.3f;
	};
	struct Enemy {
		int posX = 0;
		int posZ = 0;
		float cooldown = 0.0f;
		float rotCooldown = .1f;
		float speed = 0.0f;
		float sightRadius = 0.0f;
		float roamRadius = 0.0f;
		float baseCooldown = 0.0f;
	};
	struct EnemyDepression {
		float cooldown = 0.0f;
	};
	struct ConfigSection {
		std::string configSection;
	};
	struct Lifetime
	{
		float seconds = 0.0f;
	};

	struct Transform {
		GW::MATH::GMATRIXF matrix;
	};

	struct GameManager {};

	struct Player {
		bool hub = true;
		GW::MATH::GVECTORF direction = { 0,0,0 };
	};

	struct Firing {
		float cooldown = 0;
	};

	struct Velocity {
		GW::MATH::GVECTORF currentVelocity = { 0,0,0 };
		float speed = 0.0f;
	};

	struct Treasure {
		int lvl = -1;
		int id;
		std::string name;
	};

	struct Health {
		int amount = 0;
	};

	struct Shatters {
		int shattersRemaining = 0;
		int spawnCount = 0;
		float scale = 1.0f;
	};

	struct Invulnerability {
		float cooldown = 0;
	};
	struct PauseCooldown {
		float cooldown = 0.0f;
	};

	struct Door {

		int ID = -1;
		int dir = -1;
		int lvl = -1;

	};

	struct Level {
		GW::MATH::GMATRIXF camPosition;
		int levelNum = 0;
		bool firstEntry = true;
		bool treasureCollected = false;
		std::vector<entt::entity> destroyOnExit;
		std::vector<entt::entity> persistantEntites;
		GW::MATH::GMATRIXF invulPos;
		float timer = 0;
		bool invulSpawned = false;
		bool firstInstance = true;
	};

	struct Cover {
		int lvl = -1;
	};

	struct LevelManager {
		std::vector<Level> allLevels;
		bool ChangingCams = false;
		int oldlvl = 0;
		int newlvl = 0;
		std::vector<entt::entity> covers;
		float cameraSafetyTimer = 0;
		std::unordered_map<int, int> levelAndTreasureId;
		int hubsCompleted = 0;
	};

	// Raymond's addition: Tracks the player's score.
	struct Score
	{
		int value = 0;
	};

	// Raymond's addition: Stores an entity's previous transform so we can revert on collision.
	struct PreviousTransform
	{
		GW::MATH::GMATRIXF matrix1;
		GW::MATH::GMATRIXF matrix2;
		GW::MATH::GMATRIXF matrix3;
		GW::MATH::GMATRIXF matrix4;
		GW::MATH::GMATRIXF matrix5;
		float timer = 0.0f;
		float baseTimer = 0.05f;
	};

	struct Lives {
		int value = 0;
	};
	struct FreezeDelay {
		float cooldown = 0.0f;
	};

	// Raymond's addition: Stores the current player's name 
	struct PlayerName
	{
		std::string value = "";
	};

	// Raymond's addition: Stores the best score & name 
	struct HighScore
	{
		int value = 0;
		std::string name = "";
	};

	struct GameOver {
		bool won = false;
	};

	void SetDirection(entt::registry& registry, entt::entity entity, GW::MATH::GVECTORF direction);
	void SetSpeed(entt::registry& registry, entt::entity entity, float newSpeed);
	int ModifyHealth(entt::registry& registry, entt::entity entity, int amount);
	void ApplyInvulnerability(entt::registry& registry, entt::entity entity, std::string configSection);
	void GenerateEnemies(entt::registry& registry, std::shared_ptr<const GameConfig> config, int rAmount, int iAmount, std::vector<GW::MATH::GVECTORF> spawnLocale);
	void ChangeRooms(entt::registry& registry, entt::entity playerEntity, entt::entity doorEntity);
	int FindNewRoom(GAME::Door& door, GAME::LevelManager& lvls, entt::registry& registry);
	entt::entity FindNewDoor(GAME::Door& door, entt::registry& registry);
	void Move_Player(entt::entity doorEnt, entt::entity player, entt::registry& registry);
	void Change_Rooms(entt::entity doorEnt, entt::entity player, entt::registry& registry);
	void OnLevelEntry(int levelnum, entt::registry& registry);
	void OnLevelExit(int levelnum, entt::registry& registry);
	void PositionSelection(entt::registry& registry, entt::entity entity);
	void HubLevelSpawn(int lvl, entt::registry& registry);
	void ZoomCamera(entt::registry& registry);
	void Change_Camera(entt::registry& registry, int lvl);
	int trueLevelNum(int baseLevelNum, entt::registry& registry);
	void ClearLevelData(int lvlNum, entt::registry& registry, bool reset);
	bool CheckTreasureWinState(entt::registry& registry);
	void updateRotation(entt::registry& registry, entt::entity entity);
	void pressStart(entt::registry& registry, std::shared_ptr<const GameConfig> config, entt::entity gameManager);
	bool pauseFunction(entt::registry& registry);
	void respawn(entt::registry& registry, entt::entity gameManager);
	void ExitLevel(int lvlNum, entt::registry& registry);
}// namespace GAME
#endif // !GAME_COMPONENTS_H_
