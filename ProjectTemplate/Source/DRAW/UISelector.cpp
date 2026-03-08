#include "DrawComponents.h"
#include "../UTIL/Utilities.h"
#include "../CCL.h"
#include "../GAME/GameComponents.h"
#include "../APP/Window.hpp"
#include "../GAME/SaveScore.h"
#include "../GAME/AudioComponents.h"

namespace DRAW {
	static inline UISelectable* GetSelectableById(std::vector<UISelectable>& selectables, const std::string& selectableId) {
		for (auto& selectable : selectables) {
			if (selectable.uiElementId == selectableId) return &selectable;
		}
		return nullptr;
	}

	void MoveFocus(UI& ui, UIDirection direction) {
		UISelector& uiSelector = ui.uiSelector;
		UISelectable* currentSelection = GetSelectableById(ui.uiSelectables, uiSelector.currentlyFocusedId);
		if (currentSelection == nullptr) return;

		std::string nextSelectableId;
		switch (direction) {
		case UIDirection::UP:
			nextSelectableId = currentSelection->upId;
			break;
		case UIDirection::DOWN:
			nextSelectableId = currentSelection->downId;
			break;
		case UIDirection::LEFT:
			nextSelectableId = currentSelection->leftId;
			break;
		case UIDirection::RIGHT:
			nextSelectableId = currentSelection->rightId;
			break;
		}

		if (nextSelectableId.empty()) return;

		UISelectable* newFocusedSelectable = GetSelectableById(ui.uiSelectables, nextSelectableId);
		if (newFocusedSelectable != nullptr && newFocusedSelectable->enabled) {
			uiSelector.currentlyFocusedId = newFocusedSelectable->uiElementId;

			if (ui.nameEntry.active) {
				int slot = GetLetterSlotIndexFromId(uiSelector.currentlyFocusedId);
				if (slot != -1) ui.nameEntry.cursor = slot;
			}
		}
	}

	static inline uint8_t BumpColor(uint8_t channel, int addition) {
		int bumpedColor = channel + addition;
		return (uint8_t)(bumpedColor > 255 ? 255 : bumpedColor);
	}

	Color TintSelected(Color color) {
		color.r = BumpColor(color.r, 100);
		color.g = BumpColor(color.g, 100);
		color.b = BumpColor(color.b, 100);
		return color;
	}

	void CallStartGame(entt::registry& registry) {
		auto gameManagerView = registry.view<GAME::GameManager>();
		if (!gameManagerView.empty()) {
			entt::entity gameManager = *gameManagerView.begin();
			GAME::pressStart(registry, registry.ctx().get<UTIL::Config>().gameConfig, gameManager);
			registry.emplace<GAME::Started>(gameManager);
		}
	}

	void CallQuitGame(entt::registry& registry) {
		registry.ctx().emplace<UTIL::QuitRequested>();
	}

	void CallPause(entt::registry& registry, UI& ui) {
		bool isPaused = GAME::pauseFunction(registry);
	}

	void CallRespawn(entt::registry& registry) {
		auto gameManagerView = registry.view<GAME::GameManager>();
		if (!gameManagerView.empty()) {
			entt::entity gameManager = *gameManagerView.begin();
			GAME::respawn(registry, gameManager);
			//registry.remove<GAME::WaitingForReady>(gameManager);
			SetUIState(registry, DRAW::IN_GAME);
			
		}
	}

	void CallSwitchGameState(entt::registry& registry, std::string newState) {
		auto uiView = registry.view<UI>();
		if (uiView.empty()) return;
		entt::entity uiEntity = *uiView.begin();
		UIState newUIState = UIState::START_GAME;
		if (newState == "preInGame" || newState == "PRE_IN_GAME") {
			newUIState = UIState::PRE_IN_GAME;
		} else if (newState == "highScoreEntry" || newState == "HIGH_SCORE_ENTRY") {
			newUIState = UIState::HIGH_SCORE_ENTRY;
		}
		registry.emplace_or_replace<UIStateChange>(uiEntity, UIStateChange{ newUIState });
	}

	void CallConfirmLetter(entt::registry& registry, UI& ui) {
		if (ui.nameEntry.cursor < 2) {
			MoveFocus(ui, UIDirection::RIGHT);
		}
		else {
			ui.nameEntry.active = false;
			GAME::SaveScore::Save(ui.nameEntry.buffer, ui.nameEntry.score);
			CallSwitchGameState(registry, "START_GAME");
		}
	}

	void CallSetMusic(entt::registry& registry, const std::string& commandContext) {
		AUDIO::SetActiveSoundscape(registry, commandContext);
		AUDIO::StartMusic(registry);
	}

	void CallStopMusic(entt::registry& registry) {
		AUDIO::StopMusic(registry);
	}

	void RunCommand(entt::registry& registry, UI& ui, const UICommand& command, const std::string& uiCommandContext) {
		switch (command) {
		case UICommand::NONE:
			return;
		case UICommand::START_GAME:
			CallStartGame(registry);
			return;
		case UICommand::RESTART:
			return;
		case UICommand::QUIT:
			CallQuitGame(registry);
			return;
		case UICommand::CHANGE_GAME_STATE:
			CallSwitchGameState(registry, uiCommandContext);
			return;
		case UICommand::PAUSE:
			CallPause(registry, ui);
			return;
		case UICommand::RESPAWN:
			CallRespawn(registry);
			return;
		case UICommand::CONFIRM_LETTER:
			CallConfirmLetter(registry, ui);
			return;
		case UICommand::SET_MUSIC:
			CallSetMusic(registry, uiCommandContext);
			return;
		case UICommand::STOP_MUSIC:
			CallStopMusic(registry);
			return;
		}
		
	}

	void ActivateSelected(entt::registry& registry, UI& ui) {
		UISelector& uiSelector = ui.uiSelector;
		UISelectable* currentSelection = GetSelectableById(ui.uiSelectables, uiSelector.currentlyFocusedId);
		if (currentSelection == nullptr) return;

		RunCommand(registry, ui, currentSelection->command, currentSelection->commandContext);
	}
}