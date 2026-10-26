#include <tmpl/Version.hpp>

int main()
{
	return tmpl::version().empty() ? 1 : 0;
}