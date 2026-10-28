#pragma once
#include <charconv>
#include <chrono>
#include <concepts>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pg
{
	// Un instant UTC, à la microseconde près (la précision de TIMESTAMPTZ).
	using Timestamp = std::chrono::sys_time<std::chrono::microseconds>;

	// Une valeur au format texte de PostgreSQL. std::nullopt veut dire NULL.
	using Text = std::optional<std::string>;

	// ---- C++ vers PostgreSQL : la valeur d'un paramètre $n ----
	// Toutes les surcharges sont déclarées avant d'être définies : les modèles
	// (optional, vector) s'appellent entre eux, dans n'importe quel ordre.
	Text	toText(bool value);
	Text	toText(double value);
	Text	toText(const char *value);
	Text	toText(std::string_view value);
	Text	toText(const std::string &value);
	Text	toText(Timestamp value);
	Text	toText(std::chrono::year_month_day value);
	Text	toText(std::nullopt_t);
	template <std::integral T>
	Text	toText(T value);
	template <class Duration>
	Text	toText(std::chrono::sys_time<Duration> value);
	template <class T>
	Text	toText(const std::optional<T> &value);
	template <class T>
	Text	toText(const std::vector<T> &values);

	// ---- PostgreSQL vers C++ : false si le texte ne se lit pas ----
	bool	fromText(std::string_view text, bool &out);
	bool	fromText(std::string_view text, double &out);
	bool	fromText(std::string_view text, std::string &out);
	bool	fromText(std::string_view text, Timestamp &out);
	bool	fromText(std::string_view text, std::chrono::year_month_day &out);
	template <std::integral T>
	bool	fromText(std::string_view text, T &out);

	// Échappe une valeur pour un tableau PostgreSQL : "a\"b" pour a"b.
	std::string	quoteArrayElement(std::string_view value);

	// ---- Définitions des modèles ----

	template <std::integral T>
	Text	toText(T value)
	{
		return std::to_string(value);
	}

	template <class Duration>
	Text	toText(std::chrono::sys_time<Duration> value)
	{
		return toText(std::chrono::floor<std::chrono::microseconds>(value));
	}

	template <class T>
	Text	toText(const std::optional<T> &value)
	{
		if (!value)
			return std::nullopt;
		return toText(*value);
	}

	// {1,2,3} ; {"a b","c\"d",NULL}
	template <class T>
	Text	toText(const std::vector<T> &values)
	{
		std::string out = "{";
		for (std::size_t i = 0; i < values.size(); ++i)
		{
			if (i > 0)
				out += ',';
			const Text element = toText(values[i]);
			out += element ? quoteArrayElement(*element) : "NULL";
		}
		return out + "}";
	}

	template <std::integral T>
	bool	fromText(std::string_view text, T &out)
	{
		const char *end = text.data() + text.size();
		const auto [stop, error] = std::from_chars(text.data(), end, out);
		return error == std::errc{} && stop == end;
	}
}