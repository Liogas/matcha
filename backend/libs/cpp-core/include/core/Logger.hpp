#pragma once
#include <mutex>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace core
{
	enum class LogLevel
	{
		Debug,
		Info,
		Warning,
		Error
	};

	struct LogField
	{
		std::string key;
		std::string value;
	};

	using LogFields = std::vector<LogField>;

	class ILogger
	{
	public:
		virtual ~ILogger() = default;
		virtual void log(LogLevel level, std::string_view message, const LogFields &fields) = 0;

		void debug(std::string_view message, const LogFields &fields = {});
		void info(std::string_view message, const LogFields &fields = {});
		void warning(std::string_view message, const LogFields &fields = {});
		void error(std::string_view message, const LogFields &fields = {});
	};

	// Une ligne logfmt par message : level=INFO msg="..." key=value
	// Les champs dont la clé contient password, token ou secret sont masqués.
	class StreamLogger : public ILogger
	{
	public:
		StreamLogger(std::ostream &out, LogLevel minimum);
		void log(LogLevel level, std::string_view message, const LogFields &fields) override;

	private:
		std::ostream &_out;
		LogLevel _minimum;
		std::mutex _mutex;
	};
}