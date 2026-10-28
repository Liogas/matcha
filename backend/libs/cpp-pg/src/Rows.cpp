#include <pg/Rows.hpp>
#include <libpq-fe.h>
#include <charconv>
#include <cstring>

pg::Row::Row(std::shared_ptr<pg_result> result, int index)
	: _result(std::move(result)), _index(index)
{
}

const char *pg::Row::text(std::string_view column) const
{
	const std::string name(column); // PQfnumber veut une chaîne terminée par '\0'
	const int number = PQfnumber(this->_result.get(), name.c_str());
	if (number < 0)
		throw RowError("unknown column '" + name + "'");
	if (PQgetisnull(this->_result.get(), this->_index, number))
		return nullptr;
	return PQgetvalue(this->_result.get(), this->_index, number);
}

pg::Rows::Rows(pg_result *result)
	: _result(result, PQclear)
{
}

std::size_t pg::Rows::size() const
{
	return static_cast<std::size_t>(PQntuples(this->_result.get()));
}

bool pg::Rows::empty() const
{
	return this->size() == 0;
}

pg::Row pg::Rows::operator[](std::size_t index) const
{
	if (index >= this->size())
		throw RowError("row " + std::to_string(index) + " out of range (" + std::to_string(this->size()) + " rows)");
	return Row(this->_result, static_cast<int>(index));
}

pg::Rows::Iterator pg::Rows::begin() const
{
	return Iterator(this, 0);
}

pg::Rows::Iterator pg::Rows::end() const
{
	return Iterator(this, this->size());
}

std::int64_t pg::Rows::affected() const
{
	const char *text = PQcmdTuples(this->_result.get()); // "" si sans objet
	std::int64_t count = 0;
	std::from_chars(text, text + std::strlen(text), count);
	return count;
}

std::string_view pg::Rows::command() const
{
	return PQcmdStatus(this->_result.get());
}

pg::Rows::Iterator::Iterator(const Rows *rows, std::size_t index)
	: _rows(rows), _index(index)
{
}

pg::Row pg::Rows::Iterator::operator*() const
{
	return (*this->_rows)[this->_index];
}

pg::Rows::Iterator &pg::Rows::Iterator::operator++()
{
	++this->_index;
	return *this;
}