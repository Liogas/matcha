#pragma once

#include <auth/time/IWallClock.hpp>

namespace auth
{
	class SystemWallClock : public IWallClock
	{
		public:
			std::chrono::system_clock::time_point
				now() const override;
	};
}