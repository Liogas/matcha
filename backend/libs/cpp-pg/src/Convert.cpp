#include <pg/Convert.hpp>
#include <charconv>
#include <cstdio>

namespace
{
	// Lit exactement `count` chiffres à partir de `pos`.
	bool readDigits(std::string_view text, std::size_t pos, std::size_t count, int &out)
	{
		if (pos + count > text.size())
			return false;
		out = 0;
		for (std::size_t i = pos; i < pos + count; ++i)
		{
			if (text[i] < '0' || text[i] > '9')
				return false;
			out = out * 10 + (text[i] - '0');
		}
		return true;
	}

	// "2026-10-27" : seulement des années à 4 chiffres, pas d'avant J.-C.
	bool readDate(std::string_view text, std::chrono::year_month_day &out)
	{
		int year = 0;
		int month = 0;
		int day = 0;
		if (text.size() < 10 || text[4] != '-' || text[7] != '-' || !readDigits(text, 0, 4, year) || !readDigits(text, 5, 2, month) || !readDigits(text, 8, 2, day))
			return false;
		out = std::chrono::year{year} / std::chrono::month{static_cast<unsigned>(month)} / std::chrono::day{static_cast<unsigned>(day)};
		return out.ok();
	}
}

pg::Text pg::toText(bool value)
{
	return value ? "true" : "false";
}

pg::Text pg::toText(double value)
{
	char buffer[64];
	const auto [end, error] = std::to_chars(buffer, buffer + sizeof buffer, value);
	return std::string(buffer, end);
}

pg::Text pg::toText(const char *value)
{
	return std::string(value);
}

pg::Text pg::toText(std::string_view value)
{
	return std::string(value);
}

pg::Text pg::toText(const std::string &value)
{
	return value;
}

// ISO 8601 en UTC : 2026-10-27T08:30:00.123456Z
pg::Text pg::toText(Timestamp value)
{
	const auto day = std::chrono::floor<std::chrono::days>(value);
	const std::chrono::year_month_day date{day};
	const std::chrono::hh_mm_ss time{value - day};
	char buffer[40];
	std::snprintf(buffer, sizeof buffer, "%04d-%02u-%02uT%02d:%02d:%02d.%06dZ",
				  static_cast<int>(date.year()),
				  static_cast<unsigned>(date.month()),
				  static_cast<unsigned>(date.day()),
				  static_cast<int>(time.hours().count()),
				  static_cast<int>(time.minutes().count()),
				  static_cast<int>(time.seconds().count()),
				  static_cast<int>(time.subseconds().count()));
	return std::string(buffer);
}

pg::Text pg::toText(std::chrono::year_month_day value)
{
	char buffer[16];
	std::snprintf(buffer, sizeof buffer, "%04d-%02u-%02u",
				  static_cast<int>(value.year()),
				  static_cast<unsigned>(value.month()),
				  static_cast<unsigned>(value.day()));
	return std::string(buffer);
}

pg::Text pg::toText(std::nullopt_t)
{
	return std::nullopt;
}

std::string pg::quoteArrayElement(std::string_view value)
{
	std::string out = "\"";
	for (const char c : value)
	{
		if (c == '"' || c == '\\')
			out += '\\';
		out += c;
	}
	return out + "\"";
}

bool pg::fromText(std::string_view text, bool &out)
{
	if (text == "t")
		out = true;
	else if (text == "f")
		out = false;
	else
		return false;
	return true;
}

bool pg::fromText(std::string_view text, double &out)
{
	const char *end = text.data() + text.size();
	const auto [stop, error] = std::from_chars(text.data(), end, out);
	return error == std::errc{} && stop == end;
}

bool pg::fromText(std::string_view text, std::string &out)
{
	out = std::string(text);
	return true;
}

bool pg::fromText(std::string_view text, std::chrono::year_month_day &out)
{
	return text.size() == 10 && readDate(text, out);
}

// TIMESTAMPTZ : "2026-10-27 08:30:00.123456+00" (décalage +HH, +HH:MM ou +HH:MM:SS)
// TIMESTAMP   : "2026-10-27 08:30:00.123456"    (sans décalage : lu comme UTC)
bool pg::fromText(std::string_view text, Timestamp &out)
{
	std::chrono::year_month_day date;
	int hour = 0;
	int minute = 0;
	int second = 0;
	if (!readDate(text, date) || text.size() < 19 || (text[10] != ' ' && text[10] != 'T') || text[13] != ':' || text[16] != ':' || !readDigits(text, 11, 2, hour) || !readDigits(text, 14, 2, minute) || !readDigits(text, 17, 2, second) || hour > 23 || minute > 59 || second > 59)
		return false;
	std::size_t pos = 19;

	int micros = 0;
	if (pos < text.size() && text[pos] == '.')
	{
		int digits = 0;
		for (++pos; pos < text.size() && text[pos] >= '0' && text[pos] <= '9'; ++pos, ++digits)
			if (digits < 6)
				micros = micros * 10 + (text[pos] - '0');
		if (digits == 0 || digits > 6)
			return false;
		for (; digits < 6; ++digits)
			micros *= 10;
	}

	int offset = 0; // en secondes, à l'est de Greenwich
	if (pos < text.size() && (text[pos] == '+' || text[pos] == '-'))
	{
		const int sign = text[pos] == '-' ? -1 : 1;
		int parts[3] = {0, 0, 0};
		if (!readDigits(text, pos + 1, 2, parts[0]))
			return false;
		pos += 3;
		for (int i = 1; i < 3 && pos < text.size() && text[pos] == ':'; ++i, pos += 3)
			if (!readDigits(text, pos + 1, 2, parts[i]))
				return false;
		offset = sign * (parts[0] * 3600 + parts[1] * 60 + parts[2]);
	}
	if (pos != text.size())
		return false;

	out = std::chrono::sys_days{date} + std::chrono::hours{hour} + std::chrono::minutes{minute} + std::chrono::seconds{second} + std::chrono::microseconds{micros} - std::chrono::seconds{offset};
	return true;
}