#pragma once

#include <chrono>


namespace auth
{
	class IWallClock
	{
		public:
			virtual ~IWallClock() = default;
			virtual std::chrono::system_clock::time_point
				now() const = 0;
	};
}