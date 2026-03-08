#ifndef AUDIO_COMPONENTS_H
#define AUDIO_COMPONENTS_H

#include <soloud.h>
#include <soloud_wav.h>
#include <soloud_wavstream.h>
#include <soloud_bus.h>
#include <soloud_biquadresonantfilter.h>

#include <random>

class GameConfig;

namespace AUDIO
{
	struct MusicStemData {
		double bpm = 160.0;
		int numerator = 4;
		int denominator = 4;
		int bars = 16;
	};

	struct Stem {
		SoLoud::WavStream audio;
		int section;
		MusicStemData data;
	};

	struct ParsedStem {
		bool valid = false;
		bool isDrums = false;
		int section = 0;
		MusicStemData data{};
	};

	struct Soundscape {
		std::string name;
		std::unordered_map<int, std::vector<std::string>> sectionToStemKeys;
		std::unordered_map<std::string, std::unique_ptr<Stem>> stems;

		std::unique_ptr<Stem> drums;
		bool hasDrums = false;
	};

	class AudioController {
	public: 
		AudioController() = default;
		AudioController(const AudioController&) = delete;
		AudioController& operator=(const AudioController&) = delete;
		AudioController(AudioController&&) noexcept = default;
		AudioController& operator=(AudioController&&) noexcept = default;

		bool Init();
		void Shutdown();

		void Tick(double dtSec);
		double Now() const { return clockSeconds; }

		bool LoadSFXFromFolder(const std::filesystem::path& folder, bool recursive = true, const std::string& keyPrefix = "");
		bool LoadSFX(const std::string& key, const std::string& filePath);
		SoLoud::handle PlaySFX(const std::string& key, float volume, float pan);
		SoLoud::handle PlaySFX(const std::string& key, float volume, float pan, float pitchMin, float pitchMax);

		bool LoadSoundscapeFromFolder(const std::string& name, const std::filesystem::path& folder, MusicStemData defaults = {}, bool recursive = true);
		bool SetActiveSoundscape(const std::string& name);
		void StartActiveSoundscape(int section = 1);
		void StopActiveSoundscape(double fadeDuration = 0.25);
		void StopAll();
		void SetMasterVolume(float volume);
		void QueueSection(int section);
		void SetDrumsIntensity(float targetVolume, double fadeSeconds = 0.25);

		void Update_AudioController();
		
	private:

		void StartSoundscape(int startSection, double clockSeconds);

		int PickNextSection(int requestedSection);
		void OnSectionPlayed(int section);
		std::string PickStemForSection(int section);
		void ProcessSFXFile(const std::filesystem::path& path, const std::filesystem::path& folder, const std::string& keyPrefix, int& loadedCount, int& skippedCount);

		SoLoud::Soloud soloud;
		SoLoud::Bus musicBus;
		SoLoud::handle musicBusHandle = 0;

		SoLoud::Bus drumsBus;
		SoLoud::handle drumsBusHandle = 0;

		SoLoud::BiquadResonantFilter drumsLowPassFilter;

		std::unordered_map<std::string, SoLoud::Wav> sfx;
		std::unordered_map<std::string, std::unique_ptr<Soundscape>> soundscapes;
		Soundscape* activeSoundscape = nullptr;

		std::mt19937 rng{ std::random_device{}() };
		
		int currentSection = -1;
		double segmentStartTime = 0.0;
		double nextBoundaryTime = 0.0;

		SoLoud::handle currentMusicHandle = 0;
		SoLoud::handle drumsHandle = 0;

		float drumsTargetCutoffHz = 0.0f;
		double drumsNextBoundryTime = 0.0;
		double drumsLoopDuration = 0.0;

		std::string lastStemKey;
		int lastSection = -1;
		int repeatCount = 0;
		int pendingSection = -1;

		double clockSeconds = 0.0;
		bool soundScapePlaying = false;
	};


	//*** Tags ***//
	struct Music {};
	struct SFX {};
	struct BackgroundMusic {};
	struct ShootSound {};
	struct EnemyDeathSound {};
	struct CollectSound {};
	struct DeathSound {}; //Reffering to the PLAYER death sound effect
	struct GameStartupSound {};
	struct DoorEnterSound {};
	struct WinSound {};
	//*** Components ***//

	enum AudioVariant {
		BGMUSIC,
		SHOOT,
		ENEMY_DEATH,
		COLLECT,
		PLAYER_DEATH,
		DOOR_ENTER,
		WIN,
		GAME_STARTUP
	};

	
	void stopAllPlayingAudio(entt::registry& registry);
	void setMasterVolume(entt::registry& registry, float volume);
	void playAudio(entt::registry& registry, std::string audioName, float volume, float pan = 0.0f);
	void playAudio(entt::registry& registry, std::string audioName, float volume, float pan, float pitchMin = 1.0f, float pitchMax = 1.0f);
	void StartMusic(entt::registry& registry, int section = 1);
	void StopMusic(entt::registry& registry, double fadeDuration = 0.2);
	void SetDrumsIntensity(entt::registry& registry, float level, double fadeDuration = 0.25);
	void SetActiveSoundscape(entt::registry& registry, const std::string& soundscapeName);

}

#endif // !AUDIO_COMPONENTS_H_
