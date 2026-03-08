#pragma once
#include <string>

namespace GAME
{
	class Timer
	{
	public:
		Timer();

		// Add delta-time to the timer
		void Update(double dtSeconds);

		void StartCountdown(double startSeconds);

		void Reset(double startSeconds = 0.0);

		double GetSeconds() const;

		bool IsExpired() const;

	private:
		double seconds = 0.0;
	};
}