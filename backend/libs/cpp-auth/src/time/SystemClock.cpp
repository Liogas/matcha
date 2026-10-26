#include <auth/time/SystemClock.hpp>

namespace auth
{
	std::chrono::steady_clock::time_point 
				SystemClock::now() const
	{
		return std::chrono::steady_clock::now();
	}
}