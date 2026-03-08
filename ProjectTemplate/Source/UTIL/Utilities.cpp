#include "Utilities.h"
#include "../CCL.h"
namespace UTIL
{
	GW::MATH::GVECTORF GetRandomVelocityVector()
	{
		GW::MATH::GVECTORF vel = {float((rand() % 20) - 10), 0.0f, float((rand() % 20) - 10)};
		if (vel.x <= 0.0f && vel.x > -1.0f)
			vel.x = -1.0f;
		else if (vel.x >= 0.0f && vel.x < 1.0f)
			vel.x = 1.0f;

		if (vel.z <= 0.0f && vel.z > -1.0f)
			vel.z = -1.0f;
		else if (vel.z >= 0.0f && vel.z < 1.0f)
			vel.z = 1.0f;

		GW::MATH::GVector::NormalizeF(vel, vel);

		return vel;
	}
	void printFPS(entt::registry& registry) {
		auto& deltaTime = registry.ctx().get<DeltaTime>();
		float fps = 1.0f / deltaTime.dtSec;
		std::cout << "FPS: " << fps << std::endl;
	}

	int GetFps(entt::registry& registry) {
		auto& deltaTime = registry.ctx().get<DeltaTime>();

		static float accumulatedTime = 0.0f;
		static int accumulatedFrames = 0;
		static int displayedFps = 0;

		accumulatedTime += deltaTime.dtSec;
		accumulatedFrames++;

		if (accumulatedTime >= 0.25f) {
			float fps = (accumulatedFrames / accumulatedTime);
			displayedFps = (int)std::lround(fps);

			accumulatedTime = 0.0f;
			accumulatedFrames = 0;
		}

		return displayedFps;
	}
	
} // namespace UTIL