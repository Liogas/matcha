#include <auth/time/SystemWallClock.hpp>

namespace auth
{
	std::chrono::system_clock::time_point
		SystemWallClock::now() const
	{
		return std::chrono::system_clock::now();
	}
}