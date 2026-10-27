#include <core/Result.hpp>
#include <core/Version.hpp>

int main()
{
	const core::Result<int> answer = 42;
	return (answer && !core::version().empty()) ? 1 : 0;
}