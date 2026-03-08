#include "SaveScore.h"
#include <fstream>
#include <iostream>
#include <algorithm> 
#include <windows.h>

namespace GAME
{
	static const char* getFilePathFromAppData() {
		char* appDataPath = nullptr;
		size_t len = 0;
		errno_t err = _dupenv_s(&appDataPath, &len, "APPDATA");
		if (err || appDataPath == nullptr) {
			std::cerr << "Failed to get APPDATA environment variable." << std::endl;
			return nullptr;
		}
		std::string filePath = std::string(appDataPath) + "\\Moniscores.txt";
		free(appDataPath);
		return filePath.c_str();
	}
	static const char* SCORE_FILE = "../scores.txt";

	void SaveScore::Save(const std::string& playerName, int score)
	{
		std::ofstream file(getFilePathFromAppData(), std::ios::app);
		if (!file.is_open()) return;

		// Raymond's addition: name score 
		file << playerName << " " << score << " " << "\n";
		file.close();
	}

	std::vector<ScoreEntry> SaveScore::Load()
	{
		std::vector<ScoreEntry> scores;
		for (int i = 0; i < 10; i++) scores.push_back({ "AAA", 0 }); 

		std::ifstream file(getFilePathFromAppData());

		if (!file.is_open()) return scores;

		ScoreEntry entry;
		// Raymond's addition: read name score 
		while (file >> entry.name >> entry.score)
		{
			scores.push_back(entry);
		}

		file.close();

		// Raymond's addition: Sort scores from highest to lowest before returning
		std::sort(scores.begin(), scores.end(),
			[](const ScoreEntry& a, const ScoreEntry& b)
			{
				return a.score > b.score; // higher scores first
			});

		// Raymond's addition: Keep only top 10 scores
		if (scores.size() > 10)
		{
			scores.resize(10);

			// Raymond's addition: Rewrite file so anything NOT in top 10 is deleted
			std::ofstream outFile(getFilePathFromAppData(), std::ios::trunc);
			if (outFile.is_open())
			{
				for (const auto& s : scores)
				{
					outFile << s.name << " " << s.score << "\n";
				}
				outFile.close();
			}
		}


		return scores;
	}

	bool SaveScore::IsTopTenScore(int score) {
		std::vector<ScoreEntry> currentScores = SaveScore::Load();
		for (ScoreEntry entry : currentScores) {
			if (score > entry.score) return true;
		}
		return false;
	}
}
