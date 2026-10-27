#include <core/ConfigReader.hpp>
#include <charconv>
#include <fstream>
#include <iterator>

core::ConfigReader::ConfigReader(const IEnvironment &environment)
	: _environment(environment)
{
}

std::string	core::ConfigReader::required(const std::string &name)
{
	const auto value = this->_environment.get(name);
	if (!value)
	{
		_errors.push_back(name + " is required");
		return "";
	}
	return *value;
}

std::string	core::ConfigReader::optional(const std::string &name, const std::string &fallback)
{
	return this->_environment.get(name).value_or(fallback);
}

int	core::ConfigReader::requiredInt(const std::string &name)
{
	const auto value = this->_environment.get(name);
	if (!value)
	{
		this->_errors.push_back(name + " is required");
		return 0;
	}
	int number = 0;
	const char *end = value->data() + value->size();
	const auto [stop, error] = std::from_chars(value->data(), end, number);
	if (error != std::errc{} || stop != end)
	{
		this->_errors.push_back(name + " must be an integer, got '" + *value + "'");
		return 0;
	}
	return number;
}

bool	core::ConfigReader::optionalBool(const std::string &name, bool fallback)
{
	const auto value = this->_environment.get(name);
	if (!value)
		return fallback;
	if (*value == "true")
		return true;
	if (*value == "false")
		return false;
	this->_errors.push_back(name + " must be 'true' or 'false', got '" + *value + "'");
	return fallback;
}

std::string	core::ConfigReader::requiredSecret(const std::string &name)
{
	const std::string fileVariable = name + "_FILE";
	const auto direct = this->_environment.get(name);
	const auto path = this->_environment.get(fileVariable);
	if (direct && path)
	{
		this->_errors.push_back(name + " and " + fileVariable + " are both set, keep only one");
		return "";
	}
	if (direct)
		return *direct;
	if (!path)
	{
		this->_errors.push_back(name + " or " + fileVariable + " is required");
		return "";
	}
	std::ifstream file(*path, std::ios::binary);
	if (!file)
	{
		this->_errors.push_back(fileVariable + ": cannot read " + *path);
		return "";
	}
	std::string secret{
		std::istreambuf_iterator<char>(file),
		std::istreambuf_iterator<char>()
	};
	while (!secret.empty() && (secret.back() == '\n' || secret.back() == '\r'))
		secret.pop_back();
	if (secret.empty())
		this->_errors.push_back(fileVariable + ": " + *path + " is empty");
	return secret;
}

core::Result<void>	core::ConfigReader::finish() const
{
	if (this->_errors.empty())
		return {};
	std::string message;
	for (const auto &error : this->_errors)
	{
		if (!message.empty())
			message += "; ";
		message += error;
	}
	return fail(ErrorCode::Invalid, message);
}