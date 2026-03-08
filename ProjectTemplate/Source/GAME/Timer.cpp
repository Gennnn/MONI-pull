#include "Timer.h"
#include <sstream>
#include <iomanip>

namespace GAME
{
	Timer::Timer()
	{
		seconds = 0.0;
	}

	void Timer::StartCountdown(double startSeconds) {
		seconds = max(0, startSeconds);
	}

	void Timer::Reset(double startSeconds)
	{
		seconds = max(startSeconds, 0);
	}

	void Timer::Update(double dtSeconds)
	{
		if (dtSeconds <= 0) return;

		seconds -= dtSeconds;
		if (seconds < 0.0) seconds = 0;
	}

	double Timer::GetSeconds() const
	{
		return seconds;
	}

	bool Timer::IsExpired() const {
		return seconds <= 0.0;
	}
}