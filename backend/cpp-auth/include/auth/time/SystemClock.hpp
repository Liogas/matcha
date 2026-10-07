#pragma once

#include "IClock.hpp"

namespace auth
{
	class SystemClock : public IClock
	{
		public:
			std::chrono::steady_clock::time_point 
				now() const override;
	};
}