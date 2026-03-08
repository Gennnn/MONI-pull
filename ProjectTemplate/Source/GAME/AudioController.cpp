#include "GameComponents.h"
#include "../DRAW/DrawComponents.h"
#include "../CCL.h"
#include "ActorCreator.h"
#include "../UTIL/Utilities.h"
#include "AudioComponents.h"



namespace AUDIO {
	/*
	
	AUDIO HELPERS UTIL
	
	*/
	static double CalculateSongDuration(double bpm, int numerator, int denominator, int bars) {
		const double beatsPerSecond = 60.0 / bpm;
		const double beatsPerBar = numerator * (4.0 / denominator);
		return bars * (beatsPerSecond * beatsPerBar);
	}

	static inline std::string ToLower(std::string in) {
		for (char& character : in) character = (char)std::tolower((unsigned char)character);
		return in;
	}

	static inline void Flush(std::string& in, std::vector<std::string>& out) {
		if (!in.empty()) {
			out.push_back(ToLower(in));
			in.clear();
		}
	}

	static inline std::vector<std::string> SplitDataTokens(const std::string& name) {
		std::vector<std::string> out;
		std::string current;

		for (char character : name) {
			if (character == '_' || character == '-') Flush(current, out);
			else current.push_back(character);
		}
		Flush(current, out);
		return out;
	}

	static inline bool ParseIntAfterPrefix(const std::string& token, const char* prefix, int& out) {
		const size_t length = std::strlen(prefix);
		if (token.size() <= length) return false;
		if (token.rfind(prefix, 0) != 0) return false;
		try {
			out = std::stoi(token.substr(length));
			return true;
		}
		catch(...) {
			return false;
		}
	}

	static inline bool ParseDoubleAfterPrefix(const std::string& token, const char* prefix, double& out) {
		const size_t length = std::strlen(prefix);
		if (token.size() <= length) return false;
		if (token.rfind(prefix, 0) != 0) return false;
		try {
			out = std::stod(token.substr(length));
			return true;
		}
		catch (...) {
			return false;
		}
	}

	static bool ParseTimeSignatureToken(const std::string& token, int& outNumerator, int& outDenominator) {
		std::string tokenCopy = token;
		if (tokenCopy.rfind("ts", 0) == 0) tokenCopy = tokenCopy.substr(2);

		size_t separatorPosition = tokenCopy.find('x');
		if (separatorPosition == std::string::npos) separatorPosition = tokenCopy.find('-');
		if (separatorPosition == std::string::npos) return false;

		try {
			outNumerator = std::stoi(tokenCopy.substr(0, separatorPosition));
			outDenominator = std::stoi(tokenCopy.substr(separatorPosition + 1));
			return (outNumerator > 0 && outDenominator > 0);
		}
		catch(...) {
			return false;
		}
	}

	static ParsedStem ParseStemFromFile(const std::filesystem::path& file, MusicStemData defaults) {
		ParsedStem out;
		out.data = defaults;

		const std::string fileName = file.stem().string();
		const auto tokens = SplitDataTokens(fileName);

		bool section = false;
		bool bars = false;

		for (const std::string& token : tokens) {
			if (token == "drums" || token == "drum") {
				out.isDrums = true;
				continue;
			}

			int i = 0;
			double d = 0.0;

			if (ParseIntAfterPrefix(token, "sec", i) || ParseIntAfterPrefix(token, "s", i)) {
				out.section = i;
				section = true;
				continue;
			}
			if (ParseIntAfterPrefix(token, "bars", i) || ParseIntAfterPrefix(token, "b", i)) {
				out.data.bars = i;
				bars = true;
				continue;
			}
			if (ParseDoubleAfterPrefix(token, "bpm", d)) {
				out.data.bpm = d;
				continue;
			}

			int numerator = 0;
			int denominator = 0;

			if (ParseTimeSignatureToken(token, numerator, denominator)) {
				out.data.numerator = numerator;
				out.data.denominator = denominator;
				continue;
			}


		}

		if (out.data.bars <= 0) return out;
		if (!out.isDrums && !section) return out;

		out.valid = true;
		return out;
	}

	static inline bool IsAudioExtension(const std::filesystem::path& path) {
		const std::string extension = ToLower(path.extension().string());
		return (extension == ".wav" || extension == ".ogg" || extension == ".flac" || extension == ".mp3");
	}

	static inline void ProcessAudioFile(const std::filesystem::path& path, Soundscape& soundscape, MusicStemData& defaults) {
		if (!IsAudioExtension(path)) return;
		
		ParsedStem parsedStem = ParseStemFromFile(path, defaults);
		if (!parsedStem.valid) {
			std::cerr << "Skipping " << path.filename().string() << " could not parse data\n";
			return;
		}

		const std::string key = path.stem().string();

		if (parsedStem.isDrums) {
			soundscape.hasDrums = true;
			soundscape.drums = std::make_unique<Stem>();
			soundscape.drums->section = -1;
			soundscape.drums->data = parsedStem.data;
			if (soundscape.drums->audio.load(path.string().c_str()) != SoLoud::SO_NO_ERROR) {
				std::cerr << "Failed to load drums: " << path.string() << "\n";
				soundscape.hasDrums = false;
				soundscape.drums.reset();
			}
			
			return;
		}

		auto stemPointer = std::make_unique<Stem>();
		stemPointer->section = parsedStem.section;
		stemPointer->data = parsedStem.data;

		if (stemPointer->audio.load(path.string().c_str()) != SoLoud::SO_NO_ERROR) {
			std::cerr << "Failed to load stem: " << path.string() << "\n";
			
			return;
		}

		soundscape.sectionToStemKeys[parsedStem.section].push_back(key);
		soundscape.stems.emplace(key, std::move(stemPointer));
	}

	bool AudioController::LoadSoundscapeFromFolder(const std::string& name, const std::filesystem::path& folder, MusicStemData defaults, bool recursive) {
		Soundscape soundscape;
		soundscape.name = name;
		
		if (!std::filesystem::exists(folder)) {
			std::cerr << "Soundscape folder does not exist: " << folder.string() << "\n";
			return false;
		}

		if (recursive) {
			for (auto& fileIter : std::filesystem::recursive_directory_iterator(folder)) {
				if (fileIter.is_regular_file()) ProcessAudioFile(fileIter.path(), soundscape, defaults);
			}
		}
		else {
			for (auto& fileIter : std::filesystem::directory_iterator(folder)) {
				if (fileIter.is_regular_file()) ProcessAudioFile(fileIter.path(), soundscape, defaults);
			}
		}

		if (soundscape.stems.empty()) {
			std::cerr << "Soundscape " << name << " has no stems loaded\n";
			return false;
		}
		auto soundscapePointer = std::make_unique<Soundscape>(std::move(soundscape));
		soundscapes.insert_or_assign(name, std::move(soundscapePointer));
		return true;

	}

	static inline std::string MakeSFXKey(const std::filesystem::path& file, const std::filesystem::path& rootFolder, const std::string& keyPrefix) {
		std::filesystem::path relativePath = std::filesystem::relative(file, rootFolder);
		relativePath.replace_extension();

		std::string key = relativePath.generic_string();
		for (char& character : key) {
			if (character == '/') character = '.';
		}
		if (!keyPrefix.empty()) {
			return keyPrefix + "." + key;
		}
		return key;
	}

	

	void AudioController::ProcessSFXFile(const std::filesystem::path& path, const std::filesystem::path& folder, const std::string& keyPrefix, int& loadedCount, int& skippedCount) {
		if (!IsAudioExtension(path)) return;

		const std::string key = MakeSFXKey(path, folder, keyPrefix);

		if (sfx.find(key) != sfx.end()) {
			std::cerr << "Skipping duplicate SFX key at " << key << " from file " << path.string() << "\n";
			skippedCount++;
			return;
		}

		if (LoadSFX(key, path.string())) loadedCount++;
		else skippedCount++;
	}

	bool AudioController::LoadSFXFromFolder(const std::filesystem::path& folder, bool recursive, const std::string& keyPrefix) {
		if (!std::filesystem::exists(folder)) {
			std::cerr << "SFX folder doesn't exist: " << folder.string() << "\n";
			return false;
		}

		int loadedCount = 0;
		int skippedCount = 0;

		if (recursive) {
			for (auto& fileIter : std::filesystem::recursive_directory_iterator(folder)) {
				if (fileIter.is_regular_file()) ProcessSFXFile(fileIter, folder, keyPrefix, loadedCount, skippedCount);
			}
		}
		else {
			for (auto& fileIter : std::filesystem::directory_iterator(folder)) {
				if (fileIter.is_regular_file()) ProcessSFXFile(fileIter, folder, keyPrefix, loadedCount, skippedCount);
 			}
		}

		//std::cout << "Loaded SFX from " << folder.string() << " loadedCount=" << loadedCount << " skippedCount=" << skippedCount << "\n";

		return loadedCount > 0;
	}

	/*
	
	INIT AND SHUTDOWN

	*/

	bool AudioController::Init() {
		SoLoud::result result = soloud.init(SoLoud::Soloud::CLIP_ROUNDOFF, SoLoud::Soloud::AUTO);
		if (result != SoLoud::SO_NO_ERROR) {
			std::cerr << "SoLoud initialization failed: " << soloud.getErrorString(result) << "\n";
			return false;
		}

		musicBusHandle = soloud.play(musicBus);
		soloud.setProtectVoice(musicBusHandle, true);

		drumsBusHandle = musicBus.play(drumsBus);
		soloud.setProtectVoice(drumsBusHandle, true);

		drumsTargetCutoffHz = 17000.0f;
		drumsLowPassFilter.setParams(SoLoud::BiquadResonantFilter::LOWPASS, drumsTargetCutoffHz, 2);
		drumsBus.setFilter(0, &drumsLowPassFilter);

		return true;
	}

	void AudioController::Shutdown() {
		soloud.stopAll();
		soloud.deinit();
	}

	/*
	
	SFX FUNCTIONS
	
	*/

	bool AudioController::LoadSFX(const std::string& key, const std::string& filePath) {
		auto& soundEffect = sfx[key];
		SoLoud::result result = soundEffect.load(filePath.c_str());
		if (result == SoLoud::SO_NO_ERROR) return true;
		std::cerr << "LoadSFX failed " << key << " (" << filePath << ") :" << soloud.getErrorString(result);
		sfx.erase(key);
		return false;
	}

	SoLoud::handle AudioController::PlaySFX(const std::string& key, float volume, float pan) {
		auto sfxIterator = sfx.find(key);
		if (sfxIterator == sfx.end()) return 0;

		return soloud.play(sfxIterator->second, volume, pan);
	}

	SoLoud::handle AudioController::PlaySFX(const std::string& key, float volume, float pan, float pitchMin, float pitchMax) {
		auto sfxIterator = sfx.find(key);
		if (sfxIterator == sfx.end()) return 0;

		SoLoud::handle handle = soloud.play(sfxIterator->second, volume, pan);
		if (handle != 0 && (pitchMin != 1.0f || pitchMax != 1.0f)) {
			if (pitchMax < pitchMin) std::swap(pitchMin, pitchMax);
			std::uniform_real_distribution<float> distribution(pitchMin, pitchMax);
			soloud.setRelativePlaySpeed(handle, distribution(rng));
		}

		return handle;
	}

	/*
	
	SOUND SCAPE FUNCTIONS
	
	*/

	int AudioController::PickNextSection(int requestedSection) {
		if (activeSoundscape == nullptr) return -1;

		if (activeSoundscape->sectionToStemKeys.find(requestedSection) == activeSoundscape->sectionToStemKeys.end()) {
			requestedSection = activeSoundscape->sectionToStemKeys.begin()->first;
		}

		int maxRepeats = activeSoundscape->sectionToStemKeys.find(requestedSection)->second.size();
		maxRepeats = min(maxRepeats, 2);

		if (requestedSection == lastSection && repeatCount >= maxRepeats) {
			std::vector<int> candidates;
			candidates.reserve(activeSoundscape->sectionToStemKeys.size());
			for (auto& [section, keys] : activeSoundscape->sectionToStemKeys) {
				if (section != lastSection && !keys.empty()) candidates.push_back(section);
			}

			if (!candidates.empty()) {
				std::uniform_int_distribution<int> distribution(0, (int)candidates.size() - 1);
				return candidates[distribution(rng)];
			}
		}

		return requestedSection;
	}

	void AudioController::OnSectionPlayed(int section) {
		if (section == lastSection) repeatCount++;
		else {
			lastSection = section;
			repeatCount = 1;
		}
	}

	std::string AudioController::PickStemForSection(int section) {
		auto& keys = activeSoundscape->sectionToStemKeys.at(section);
		if (keys.empty()) return {};
		if (keys.size() == 1) return keys[0];
		std::uniform_int_distribution<int> distribution(0, (int)keys.size() - 1);
		for (int tries = 0; tries < 8; tries++) {
			const std::string& k = keys[distribution(rng)];
			if (k != lastStemKey) return k;
		}
		for (auto& k : keys) if (k != lastStemKey) return k;
		return keys[0];
	}

	

	void AudioController::StartSoundscape(int startSection, double clockSeconds) {
		if (activeSoundscape == nullptr) return;

		if (activeSoundscape->sectionToStemKeys.find(startSection) == activeSoundscape->sectionToStemKeys.end()) {
			startSection = activeSoundscape->sectionToStemKeys.begin()->first;
		}

		const double eps = 0.05;
		segmentStartTime = clockSeconds + eps;

		const int chosenSection = PickNextSection(startSection);
		const std::string key = PickStemForSection(chosenSection);
		lastStemKey = key;
		auto& stem = *activeSoundscape->stems.at(key);

		currentMusicHandle = musicBus.playClocked(segmentStartTime, stem.audio, 1.0f, 0.0f);
		currentSection = chosenSection;
		OnSectionPlayed(chosenSection);

		const double duration = CalculateSongDuration(stem.data.bpm, stem.data.numerator, stem.data.denominator, stem.data.bars);
		nextBoundaryTime = segmentStartTime + duration;

		if (activeSoundscape->hasDrums) {
			auto& drums = *activeSoundscape->drums;
			drumsLoopDuration = CalculateSongDuration(drums.data.bpm, drums.data.numerator, drums.data.denominator, drums.data.bars);
			
			drumsHandle = drumsBus.playClocked(segmentStartTime, drums.audio, 1.0f, 0.0f);
			drumsNextBoundryTime = segmentStartTime + drumsLoopDuration;
			drumsTargetCutoffHz = 17000.0f;
			drumsLowPassFilter.setParams(SoLoud::BiquadResonantFilter::LOWPASS, drumsTargetCutoffHz, 2);
			
		}
	}

	void AudioController::StartActiveSoundscape(int section) {
		if (!soloud.isValidVoiceHandle(musicBusHandle)) {
			musicBusHandle = soloud.play(musicBus);
			soloud.setProtectVoice(musicBusHandle, true);
		}
		StartSoundscape(section, clockSeconds);
		soundScapePlaying = true;
	}

	void AudioController::QueueSection(int section) {
		pendingSection = section;
	}

	bool AudioController::SetActiveSoundscape(const std::string& name) {
		auto soundscapesIter = soundscapes.find(name);
		if (soundscapesIter == soundscapes.end()) return false;
	
		activeSoundscape = soundscapesIter->second.get();

		currentSection = -1;
		lastSection = -1;
		repeatCount = 0;
		pendingSection = -1;

		currentMusicHandle = 0;
		drumsHandle = 0;
		drumsTargetCutoffHz = 17000.0f;

		return true;
	}

	void AudioController::Update_AudioController() {
		if (!soundScapePlaying || activeSoundscape ==  nullptr || currentMusicHandle == 0) return;
		const double scheduleAhead = 0.10;

		if (clockSeconds + scheduleAhead >= nextBoundaryTime) {
			int requested = (pendingSection >= 0) ? pendingSection : currentSection;
			requested = PickNextSection(requested);

			const std::string nextKey = PickStemForSection(requested);
			auto& nextStem = activeSoundscape->stems.at(nextKey);


			SoLoud::handle nextHandle = musicBus.playClocked(nextBoundaryTime, nextStem->audio);

			currentMusicHandle = nextHandle;
			currentSection = requested;
			pendingSection = -1;
			OnSectionPlayed(requested);

			segmentStartTime = nextBoundaryTime;
			const double newDuration = CalculateSongDuration(nextStem->data.bpm, nextStem->data.numerator, nextStem->data.denominator, nextStem->data.bars);
			nextBoundaryTime = segmentStartTime + newDuration;
		}

		if (activeSoundscape->hasDrums && drumsHandle != 0 && clockSeconds + scheduleAhead >= drumsNextBoundryTime) {
			auto& drums = activeSoundscape->drums;
			SoLoud::handle nextDrums = drumsBus.playClocked(drumsNextBoundryTime, drums->audio);
			drumsHandle = nextDrums;
			drumsNextBoundryTime += drumsLoopDuration;
		}
	}

	void AudioController::Tick(double dtSec) {
		clockSeconds += dtSec;
		Update_AudioController();
	}

	static inline void StopHandle(SoLoud::handle& handle, SoLoud::Soloud& soloud, double fadeDuration, double stopAt) {
		if (handle == 0) return;
		if (soloud.isValidVoiceHandle(handle)) {
			if (fadeDuration > 0) {
				soloud.fadeVolume(handle, 0.0f, fadeDuration);
				soloud.scheduleStop(handle, stopAt);
			}
			else {
				soloud.stop(handle);
			}
		}
		handle = 0;
	}

	void AudioController::StopActiveSoundscape(double fadeDuration) {
		soundScapePlaying = false;
		pendingSection = -1;

		const double stopAt = clockSeconds + max(0.0, fadeDuration);
		StopHandle(currentMusicHandle, soloud, fadeDuration, stopAt);
		StopHandle(drumsHandle, soloud, fadeDuration, stopAt);

		currentSection = -1;
		lastSection = -1;
		repeatCount = 0;
	}

	void AudioController::StopAll() {
		soloud.stopAll();

		musicBusHandle = soloud.play(musicBus);
		soloud.setProtectVoice(musicBusHandle, true);

		drumsBusHandle = musicBus.play(drumsBus);
		soloud.setProtectVoice(drumsBusHandle, true);

		drumsTargetCutoffHz = 17000.0f;
		drumsLowPassFilter.setParams(SoLoud::BiquadResonantFilter::LOWPASS, drumsTargetCutoffHz, 2.0f);
		drumsBus.setFilter(0, &drumsLowPassFilter);

		currentMusicHandle = 0;
		drumsHandle = 0;
		pendingSection = -1;
	}

	void AudioController::SetMasterVolume(float volume) {
		if (volume < 0) volume = 0;
		if (volume > 1) volume = 1;
		soloud.setGlobalVolume(volume);
	}

	void AudioController::SetDrumsIntensity(float targetCutoffHz, double fadeSeconds) {
		drumsTargetCutoffHz = targetCutoffHz;
		if (soloud.isValidVoiceHandle(drumsBusHandle)) {
			soloud.fadeFilterParameter(drumsBusHandle, 0, SoLoud::BiquadResonantFilter::FREQUENCY, drumsTargetCutoffHz, fadeSeconds);
		}
	}

	void stopAllPlayingAudio(entt::registry& registry) {
		auto view = registry.view<GW::AUDIO::GSound>();
		for (auto entity : view) {
			GW::AUDIO::GSound& sound = registry.get<GW::AUDIO::GSound>(entity);
			sound.Stop();
		}
		auto musicView = registry.view<GW::AUDIO::GMusic>();
		for (auto entity : musicView) {
			GW::AUDIO::GMusic& music = registry.get<GW::AUDIO::GMusic>(entity);
			music.Stop();
		}

		if (registry.ctx().contains<AUDIO::AudioController>()) {
			registry.ctx().get<AUDIO::AudioController>().StopAll();
		}

	}

	void setMasterVolume(entt::registry& registry, float volume) {
		/*GW::AUDIO::GAudio& audioController = registry.ctx().get<GW::AUDIO::GAudio>();
		auto GReturn = audioController.SetMasterVolume(volume);
		if (GReturn != GW::GReturn::SUCCESS) {
			std::cout << "Error setting master volume!" << std::endl;
		}*/
		if (registry.ctx().contains<AUDIO::AudioController>()) {
			registry.ctx().get<AUDIO::AudioController>().SetMasterVolume(volume);
		}

	}
	
	void playAudio(entt::registry& registry, std::string audioName, float volume, float pan) {
		if (registry.ctx().contains<AUDIO::AudioController>()) {
			registry.ctx().get<AUDIO::AudioController>().PlaySFX(audioName.c_str(), volume, pan);
		}
	}

	void playAudio(entt::registry& registry, std::string audioName, float volume, float pan, float pitchMin, float pitchMax) {
		if (registry.ctx().contains<AUDIO::AudioController>()) {
			registry.ctx().get<AUDIO::AudioController>().PlaySFX(audioName.c_str(), volume, pan, pitchMin, pitchMax);
		}
	}

	void StartMusic(entt::registry& registry, int section) {
		if (registry.ctx().contains<AUDIO::AudioController>()) {
			registry.ctx().get<AUDIO::AudioController>().StartActiveSoundscape(section);
		}
	}

	void StopMusic(entt::registry& registry, double fadeDuration) {
		if (registry.ctx().contains<AUDIO::AudioController>()) {
			registry.ctx().get<AUDIO::AudioController>().StopActiveSoundscape(fadeDuration);
		}
	}

	void SetDrumsIntensity(entt::registry& registry, float level, double fadeDuration) {
		if (registry.ctx().contains<AUDIO::AudioController>()) {
			registry.ctx().get<AUDIO::AudioController>().SetDrumsIntensity(level, fadeDuration);
		}
	}

	void SetActiveSoundscape(entt::registry& registry, const std::string& soundscapeName) {
		if (registry.ctx().contains<AUDIO::AudioController>()) {
			registry.ctx().get<AUDIO::AudioController>().SetActiveSoundscape(soundscapeName);
		}
	}
}