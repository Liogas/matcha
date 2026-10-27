#include <core/Version.hpp>

namespace core
{
	std::string_view	version() noexcept
	{
		return CPP_CORE_VERSION;
	}
}