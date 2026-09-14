#include "QueryResult.hpp"
#include <iostream>
#include <charconv>
#include <cstring>

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

bool	QueryResult::isError() const // CONDITION MODIFIEE A TESTER
{
	if (this->_result && (this->isCommand() || this->isTuples()))
		return (false);
	return (true);
}

bool	QueryResult::isCommand() const // CONDITION MODIFIEE A TESTER
{
	if (this->_result && PQresultStatus(this->_result) == PGRES_COMMAND_OK)
		return (true);
	return (false);
}

bool	QueryResult::isTuples() const // CONDITION MODIFIEE A TESTER
{
	if (this->_result && PQresultStatus(this->_result) == PGRES_TUPLES_OK)
		return (true);
	return (false);
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
		return {}; // Revient au meme que std::nullopt
	return std::string(this->value(row, column));
}

std::optional<int>	QueryResult::getInt(int row, int column) const
{
	if (!this->_result || this->isNull(row, column))
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

bool	QueryResult::isValidPosition(int row, int column) const
{
	if (!this->_result)
		return (false);
	return row >= 0
		&& row < this->rowCount()
		&& column >= 0
		&& column < this->columnCount();
}
