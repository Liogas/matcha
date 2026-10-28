#include <pg/Version.hpp>

namespace pg
{
	std::string_view	version() noexcept
	{
		return CPP_PG_VERSION;
	}
}