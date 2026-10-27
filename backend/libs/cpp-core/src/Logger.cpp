#include <core/Logger.hpp>
#include <algorithm>
#include <array>
#include <cctype>

namespace
{
	std::string_view levelName(core::LogLevel level)
	{
		switch (level)
		{
		case core::LogLevel::Debug:
			return "DEBUG";
		case core::LogLevel::Info:
			return "INFO";
		case core::LogLevel::Warning:
			return "WARNING";
		case core::LogLevel::Error:
			return "ERROR";
		}
		return "ERROR";
	}

	bool isSensitive(std::string key)
	{
		std::transform(key.begin(), key.end(), key.begin(),
					   [](unsigned char c)
					   { return static_cast<char>(std::tolower(c)); });
		constexpr std::array<std::string_view, 3> words{"password", "token", "secret"};
		return std::any_of(words.begin(), words.end(),
						   [&key](std::string_view word)
						   { return key.find(word) != std::string::npos; });
	}

	// Entre guillemets si besoin, et sans retour à la ligne : un message ne
	// peut pas fabriquer une fausse ligne de log.
	std::string quote(std::string_view value)
	{
		const bool plain = !value.empty() && std::all_of(value.begin(), value.end(),
														 [](unsigned char c)
														 { return std::isgraph(c) && c != '"' && c != '=' && c != '\\'; });
		if (plain)
			return std::string{value};
		std::string out = "\"";
		for (const char c : value)
		{
			switch (c)
			{
			case '"':
				out += "\\\"";
				break;
			case '\\':
				out += "\\\\";
				break;
			case '\n':
				out += "\\n";
				break;
			case '\r':
				out += "\\r";
				break;
			case '\t':
				out += "\\t";
				break;
			default:
				out += c;
			}
		}
		return out + "\"";
	}
}

void core::ILogger::debug(std::string_view message, const LogFields &fields)
{
	log(LogLevel::Debug, message, fields);
}

void core::ILogger::info(std::string_view message, const LogFields &fields)
{
	log(LogLevel::Info, message, fields);
}

void core::ILogger::warning(std::string_view message, const LogFields &fields)
{
	log(LogLevel::Warning, message, fields);
}

void core::ILogger::error(std::string_view message, const LogFields &fields)
{
	log(LogLevel::Error, message, fields);
}

core::StreamLogger::StreamLogger(std::ostream &out, LogLevel minimum)
	: _out(out), _minimum(minimum)
{
}

void core::StreamLogger::log(LogLevel level, std::string_view message, const LogFields &fields)
{
	if (level < this->_minimum)
		return;
	std::string line = "level=" + std::string{levelName(level)} + " msg=" + quote(message);
	for (const auto &field : fields)
		line += " " + field.key + "=" + (isSensitive(field.key) ? "***" : quote(field.value));
	line += '\n';
	const std::lock_guard lock(this->_mutex);
	this->_out << line << std::flush;
}