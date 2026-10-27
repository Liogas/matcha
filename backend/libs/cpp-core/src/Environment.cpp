#include <core/Environment.hpp>
#include <cstdlib>
#include <utility>

std::optional<std::string> core::SystemEnvironment::get(const std::string &name) const
{
	const char *value = std::getenv(name.c_str());
	if (value == nullptr || *value == '\0')
		return std::nullopt;
	return std::string{value};
}

void core::MapEnvironment::set(const std::string &name, std::string value)
{
	this->_values[name] = std::move(value);
}

std::optional<std::string> core::MapEnvironment::get(const std::string &name) const
{
	const auto found = this->_values.find(name);
	if (found == this->_values.end() || found->second.empty())
		return std::nullopt;
	return found->second;
}