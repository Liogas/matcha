#include "QueryResult.hpp"
#include <iostream>
#include <charconv>
#include <cstring>
#include <catalog/pg_type_d.h>
#include <cstdio>
#include <ctime>
#include <string_view>

QueryResult::QueryResult():
	_result(nullptr)
{
	std::cout << "[LOG BDD] Result nullptr created" << std::endl;
}

QueryResult::QueryResult(PGresult *r):
	_result(r)
{
	std::cout << "[LOG BDD] Result created" << std::endl;
}

QueryResult::QueryResult(QueryResult &&other) noexcept:
	_result(other._result)
{
	other._result = nullptr;
}

QueryResult::~QueryResult()
{
	if (this->_result)
	{
		PQclear(this->_result);
		std::cout << "[LOG BDD] Result destroyed" << std::endl;
	}
}

QueryResult	&QueryResult::operator=(QueryResult &&other) noexcept
{
	if (this != &other)
	{
		if (this->_result)
			PQclear(this->_result);
		this->_result = other._result;
		other._result = nullptr;
	}
	return *this;
}

PGresult	*QueryResult::get() const
{
	return (this->_result);
}

bool	QueryResult::isError() const
{
	if (this->_result && (this->isCommand() || this->isTuples()))
		return (false);
	return (true);
}

bool	QueryResult::isCommand() const
{
	if (this->_result && PQresultStatus(this->_result) == PGRES_COMMAND_OK)
		return (true);
	return (false);
}

bool	QueryResult::isTuples() const
{
	if (this->_result && PQresultStatus(this->_result) == PGRES_TUPLES_OK)
		return (true);
	return (false);
}

std::string	QueryResult::sqlState() const
{
	if (!this->_result)
		return {};
	const char *state = PQresultErrorField(
		this->_result,
		PG_DIAG_SQLSTATE
	);
	if (state == nullptr)
		return {};
	return state;
}

int	QueryResult::columnCount() const
{
	if (!this->_result)
		return (0);
	return (PQnfields(this->_result));
}

int	QueryResult::rowCount() const
{
	if (!this->_result)
		return (0);
	return (PQntuples(this->_result));
}

const char	*QueryResult::columnName(int column) const
{
	if (!this->_result || column < 0 || column >= this->columnCount())
		return (nullptr);
	return (PQfname(this->_result, column));
}

const char	*QueryResult::value(int row, int column) const
{
	if (!this->_result || !this->isValidPosition(row, column))
		return (nullptr);
	return (PQgetvalue(this->_result, row, column));
}

bool		QueryResult::isNull(int row, int column) const
{
	if (!this->_result || !this->isValidPosition(row, column)
		|| PQgetisnull(this->_result, row, column) == 1)
		return (true);
	return (false);
}

Oid			QueryResult::columnType(int column) const
{
	if (!this->_result || column < 0 || column >= this->columnCount())
		return (InvalidOid);
	return (PQftype(this->_result, column));
}

std::optional<std::string>	QueryResult::getString(int row, int column) const
{
	if (!this->_result || this->isNull(row, column))
		return std::nullopt;
	Oid type = this->columnType(column);
	if (type != TEXTOID && type != VARCHAROID && type != BPCHAROID)
		return std::nullopt;
	return std::string(this->value(row, column));
}

std::optional<int>	QueryResult::getInt(int row, int column) const
{
	if (!this->_result || this->isNull(row, column))
		return std::nullopt;
	Oid type = this->columnType(column);
	if (type != INT2OID && type != INT4OID)
		return std::nullopt;

	const char *value	= this->value(row, column);
	const char *end		= value + std::strlen(value);
	int result			= 0;
	
	auto [ptr, error] 	= std::from_chars(
		value,
		value + std::strlen(value),
		result
	);
	if (error != std::errc() || ptr != end)
		return std::nullopt;
	return (result);
}

std::optional<std::int64_t>	QueryResult::getInt64(int row, int column) const
{
	if (!this->_result || this->isNull(row, column))
		return std::nullopt;
	Oid type = this->columnType(column);
	if (type != INT8OID)
		return std::nullopt;

	const char *value	= this->value(row, column);
	const char *end		= value + std::strlen(value);
	std::int64_t result	= 0;
	
	auto [ptr, error] 	= std::from_chars(
		value,
		value + std::strlen(value),
		result
	);
	if (error != std::errc() || ptr != end)
		return std::nullopt;
	return (result);
}

std::optional<bool>	QueryResult::getBool(int row, int column) const
{
	if (!this->_result || this->isNull(row, column))
		return std::nullopt;
	if (this->columnType(column) != BOOLOID)
		return std::nullopt;
	const char *value = this->value(row, column);
	if (std::strcmp(value, "t") == 0)
		return (true);
	if (std::strcmp(value, "f") == 0)
		return (false);
	return (std::nullopt);
}

std::optional<std::chrono::year_month_day>	QueryResult::getDate(int row, int column) const
{
	if (!this->_result || this->isNull(row, column))
		return std::nullopt;
	if (this->columnType(column) != DATEOID)
		return std::nullopt;
	const char *value = this->value(row, column);
	if (!value)
		return std::nullopt;
	int year = 0;
	int day = 0;
	int month = 0;
	if (std::sscanf(value, "%d-%d-%d", &year, &month, &day) != 3)
		return std::nullopt;
	std::chrono::year_month_day date{
		std::chrono::year{year},
		std::chrono::month{static_cast<unsigned>(month)},
		std::chrono::day{static_cast<unsigned>(day)}
	};
	if (!date.ok())
		return std::nullopt;
	return (date);
}

std::optional<std::chrono::sys_time<std::chrono::microseconds>>	QueryResult::getTimestamp(int row, int column) const
{
	if (!this->_result || this->isNull(row, column))
		return std::nullopt;
	if (this->columnType(column) != TIMESTAMPOID)
		return std::nullopt;
	const char *value = this->value(row, column);
	if (!value)
		return std::nullopt;
	std::string_view input(value);
	if (input.size() < 19)
		return std::nullopt;
	int year = 0;
	unsigned int month = 0;
	unsigned int day = 0;
	unsigned int hour = 0;
	unsigned int minute = 0;
	unsigned int second = 0;
	auto parseInt = [](std::string_view value, auto &result)
	{
		const char *begin = value.data();
		const char *end = begin + value.size();
		auto [ptr, error] = std::from_chars(
			begin,
			end,
			result
		);
		return error == std::errc() && ptr == end;
	};
	if (input[4] != '-'
		|| input[7] != '-'
		|| input[10] != ' '
		|| input[13] != ':'
		|| input[16] != ':'
	)
		return std::nullopt;
	if (!parseInt(input.substr(0, 4), year)
		|| !parseInt(input.substr(5, 2), month)
		|| !parseInt(input.substr(8, 2), day)
		|| !parseInt(input.substr(11, 2), hour)
		|| !parseInt(input.substr(14, 2), minute)
		|| !parseInt(input.substr(17, 2), second))
		return std::nullopt;
	std::chrono::year_month_day date {
		std::chrono::year{year},
		std::chrono::month{month},
		std::chrono::day{day}
	};
	if (!date.ok() || hour > 23 || minute > 59 || second > 59)
		return std::nullopt;
	auto time = std::chrono::sys_days{date}
		+ std::chrono::hours{hour}
		+ std::chrono::minutes{minute}
		+ std::chrono::seconds{second};
	return std::chrono::time_point_cast<std::chrono::microseconds>(time);
}

bool	QueryResult::isValidPosition(int row, int column) const
{
	if (!this->_result)
		return (false);
	return row >= 0
		&& row < this->rowCount()
		&& column >= 0
		&& column < this->columnCount();
}
