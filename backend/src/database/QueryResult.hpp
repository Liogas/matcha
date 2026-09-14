#pragma once

#include <libpq-fe.h>
#include <string>
#include <optional>

class QueryResult
{
	public:
		QueryResult();
		explicit QueryResult(PGresult *result);
		QueryResult(const QueryResult &) = delete;
		QueryResult(QueryResult &&other) noexcept;
		~QueryResult();

		QueryResult &operator=(const QueryResult &) = delete;
		QueryResult	&operator=(const QueryResult &&other) noexcept;

		PGresult	*get() const;
		bool		isError() const;
		bool		isTuples() const;
		bool		isCommand() const;

		int			rowCount() const;
		int			columnCount() const;

		const char	*columnName(int column) const;
		const char	*value(int row, int column) const;
		bool		isNull(int row, int column) const;
		Oid			columnType(int column) const;

		std::optional<std::string>	getString(int row, int column) const;
		std::optional<int>			getInt(int row, int column) const;

		bool	isValidPosition(int row, int column) const;
	private:
		PGresult	*_result;
};