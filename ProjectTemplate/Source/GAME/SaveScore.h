#pragma once
#include <string>
#include <vector>

namespace GAME
{
	struct ScoreEntry
	{
		std::string name;
		int score;

	};

	// Raymond's addition: Handles saving and loading high scores
	class SaveScore
	{
	public:
		static void Save(const std::string& playerName, int score);
		static std::vector<ScoreEntry> Load();
		static bool IsTopTenScore(int score);
	};
}
