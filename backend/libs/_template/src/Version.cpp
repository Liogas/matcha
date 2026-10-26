#include <tmpl/Version.hpp>

namespace tmpl
{
	std::string_view	version() noexcept
	{
		return CPP_TEMPLATE_VERSION;
	}
}