// main entry point for the application
// enables components to define their behaviors locally in an .hpp file
#include "CCL.h"
#include "UTIL/Utilities.h"
// include all components, tags, and systems used by this program
#include "DRAW/DrawComponents.h"
#include "GAME/AudioComponents.h"
#include "GAME/GameComponents.h"
#include "APP/Window.hpp"
#include "GAME/ActorCreator.h"


// Local routines for specific application behavior
void GraphicsBehavior(entt::registry& registry);
void GameplayBehavior(entt::registry& registry);
void MainLoopBehavior(entt::registry& registry);

// Architecture is based on components/entities pushing updates to other components/entities (via "patch" function)
int main()
{

	// All components, tags, and systems are stored in a single registry
	entt::registry registry;	

	// initialize the ECS Component Logic
	CCL::InitializeComponentLogic(registry);

	// Seed the rand
	unsigned int time = std::chrono::steady_clock::now().time_since_epoch().count();
	srand(time);

	registry.ctx().emplace<UTIL::Config>();

	GraphicsBehavior(registry); // create windows, surfaces, and renderers

	GameplayBehavior(registry); // create entities and components for gameplay
	//Get original GPUInstance for enemy
	
	MainLoopBehavior(registry); // update windows and input

	// clear all entities and components from the registry
	// invokes on_destroy() for all components that have it
	// registry will still be intact while this is happening
	registry.clear(); 

	return 0; // now destructors will be called for all components
}

// This function will be called by the main loop to update the graphics
// It will be responsible for loading the Level, creating the VulkanRenderer, and all VulkanInstances
void GraphicsBehavior(entt::registry& registry)
{
	std::shared_ptr<const GameConfig> config = registry.ctx().get<UTIL::Config>().gameConfig;

	// Add an entity to handle all the graphics data
	auto display = registry.create();

	// TODO: Emplace CPULevel. Placing here to reduce occurrence of a json race condition crash
	std::string levelPath = (*config).at("Level1").at("levelFile").as<std::string>();
	std::string modelPath = (*config).at("Level1").at("modelPath").as<std::string>();
	registry.emplace<DRAW::CPULevel>(display, DRAW::CPULevel{levelPath, modelPath});
	// Emplace and initialize Window component
	int windowWidth = (*config).at("Window").at("width").as<int>();
	int windowHeight = (*config).at("Window").at("height").as<int>();
	int startX = (*config).at("Window").at("xstart").as<int>();
	int startY = (*config).at("Window").at("ystart").as<int>();
	registry.emplace<APP::Window>(display,
		APP::Window{ startX, startY, windowWidth, windowHeight, GW::SYSTEM::GWindowStyle::WINDOWEDBORDERED, "MONI"});


	// Create the input
	auto& input =  registry.ctx().emplace<UTIL::Input>();
	auto& modelManager = registry.ctx().emplace<DRAW::ModelManager>();
	auto& window = registry.get<GW::SYSTEM::GWindow>(display);
	input.bufferedInput.Create(window);
	input.immediateInput.Create(window);
	input.gamePads.Create();
	auto& pressEvents = registry.ctx().emplace<GW::CORE::GEventCache>();
	pressEvents.Create(32);
	input.bufferedInput.Register(pressEvents);
	input.gamePads.Register(pressEvents);

	// Create a transient component to initialize the Renderer
	std::string vertShader = (*config).at("Shaders").at("vertex").as<std::string>();
	std::string pixelShader = (*config).at("Shaders").at("pixel").as<std::string>();
	std::string uiVertexShader = (*config).at("Shaders").at("uiVertex").as<std::string>(); //("../Shaders/UIVertexShader.hlsl";
	std::string uiPixelShader = (*config).at("Shaders").at("uiPixel").as<std::string>();
	std::string skyboxVertexShader = (*config).at("Shaders").at("skyboxVertex").as<std::string>(); 
	std::string skyboxPixelShader = (*config).at("Shaders").at("skyboxPixel").as<std::string>();
	registry.emplace<DRAW::VulkanRendererInitialization>(display,
		DRAW::VulkanRendererInitialization{ 
			vertShader, pixelShader, uiVertexShader, uiPixelShader, skyboxVertexShader, skyboxPixelShader,
			{ {0.2f, 0.2f, 0.25f, 1} } , { 1.0f, 0u }, 75.f, 0.1f, 500.0f });
	registry.emplace<DRAW::VulkanRenderer>(display);
	
	// TODO : Emplace GPULevel
	registry.emplace<DRAW::GPULevel>(display, DRAW::GPULevel{});
	// Register for Vulkan clean up
	GW::CORE::GEventResponder shutdown;
	shutdown.Create([&](const GW::GEvent& e) {
		GW::GRAPHICS::GVulkanSurface::Events event;
		GW::GRAPHICS::GVulkanSurface::EVENT_DATA data;
		if (+e.Read(event, data) && event == GW::GRAPHICS::GVulkanSurface::Events::RELEASE_RESOURCES) {
			registry.clear<DRAW::VulkanRenderer>();
		}
		});
	registry.get<DRAW::VulkanRenderer>(display).vlkSurface.Register(shutdown);
	registry.emplace<GW::CORE::GEventResponder>(display, shutdown.Relinquish());

	// Create a camera and emplace it
	GW::MATH::GMATRIXF initialCamera;
	GW::MATH::GVECTORF translate = { 0.0f,  45.0f, -5.0f };
	GW::MATH::GVECTORF lookat = { 0.0f, 0.0f, 0.0f };
	GW::MATH::GVECTORF up = { 0.0f, 1.0f, 0.0f };
	GW::MATH::GMatrix::TranslateGlobalF(initialCamera, translate, initialCamera);
	GW::MATH::GMatrix::LookAtLHF(translate, lookat, up, initialCamera);
	// Inverse to turn it into a camera matrix, not a view matrix. This will let us do
	// camera manipulation in the component easier
	GW::MATH::GMatrix::InverseF(initialCamera, initialCamera);
	registry.emplace<DRAW::Camera>(display,
		DRAW::Camera{ initialCamera });
}




// This function will be called by the main loop to update the gameplay
// It will be responsible for updating the VulkanInstances and any other gameplay components
void GameplayBehavior(entt::registry& registry)
{
	std::shared_ptr<const GameConfig> config = registry.ctx().get<UTIL::Config>().gameConfig;

	// Raymond's addition: Create the GameManager entity.
	entt::entity gameManager = registry.create();
	registry.emplace<GAME::GameManager>(gameManager);

	// Raymond's addition: Initialize score to 0 on the GameManager.
	registry.emplace<GAME::Score>(gameManager, 0);

	// Raymond's addition: Initialize player name + high score 
	registry.emplace<GAME::PlayerName>(gameManager);
	registry.emplace<GAME::HighScore>(gameManager);

	auto& audio = registry.ctx().emplace<AUDIO::AudioController>();

	audio.Init();

	//audio.LoadSoundscapeFromFolder("menu", "../Audio/music/menu");
	audio.LoadSoundscapeFromFolder("gameplay", "../Audio/music/gameplay/");
	audio.LoadSoundscapeFromFolder("menu", "../Audio/music/menu/");
	audio.LoadSFXFromFolder("../Audio/sfx/");

	/*audio.SetActiveSoundscape("menu");
	audio.StartActiveSoundscape(1);*/
	//audio.StartActiveSoundscape();

}

// This function will be called by the main loop to update the main loop
// It will be responsible for updating any created windows and handling any input
void MainLoopBehavior(entt::registry& registry)
{	
	// main loop
	int closedCount; // count of closed windows
	auto winView = registry.view<APP::Window>(); // for updating all windows
	auto& deltaTime = registry.ctx().emplace<UTIL::DeltaTime>().dtSec;
	
	// for updating all windows
	do {
		// Set the delta time
		static auto start = std::chrono::steady_clock::now();
		double elapsed = std::chrono::duration<double>(
			std::chrono::steady_clock::now() - start).count();
		start = std::chrono::steady_clock::now();

		registry.ctx().get<AUDIO::AudioController>().Tick(elapsed);

		// Cap delta time to min 30 fps. This will prevent too much time from simulating when dragging the window
		if(elapsed > 1.0 / 30.0)
		{
			elapsed = 1.0 / 30.0;
		}
		deltaTime = elapsed;

		if (registry.ctx().contains<UTIL::QuitRequested>() && !registry.ctx().get<UTIL::QuitRequested>().value) {
			auto vulkanRendererView = registry.view<DRAW::VulkanRenderer>();
			for (auto entity : vulkanRendererView) {
				registry.remove<DRAW::VulkanRenderer>(entity);
			}

			for (auto entity : winView) {
				registry.emplace_or_replace<APP::WindowClosed>(entity);
			}
			registry.ctx().get<AUDIO::AudioController>().Shutdown();
			registry.ctx().get<UTIL::QuitRequested>().value = true;
		}

		// TODO : Update Game
		auto gameManagerView = registry.view<GAME::GameManager>();
		if (!gameManagerView.empty()) {
			entt::entity gameManager = *gameManagerView.begin();
			registry.patch<GAME::GameManager>(gameManager);
		}
		closedCount = 0;
		// find all Windows that are not closed and call "patch" to update them
		for (auto entity : winView) {
			if (registry.any_of<APP::WindowClosed>(entity))
				++closedCount;
			else
				registry.patch<APP::Window>(entity); // calls on_update()
		}
	} while (winView.size() != closedCount); // exit when all windows are closed
}
