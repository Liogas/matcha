#include <pg/QueryBuilder.hpp>
#include <stdexcept>
#include <utility>

pg::QueryBuilder::QueryBuilder(std::string select):
	_select(std::move(select))
{}

void pg::QueryBuilder::addCondition(
	std::string_view condition,
	std::size_t first,
	std::size_t count
)
{
	std::string numbered;
	std::size_t next = first;
	for (const char character : condition)
	{
		if (character != '?')
		{
			numbered += character;
			continue;
		}
		numbered += '$' + std::to_string(next);
		++next;
	}
	if (next - first != count)
		throw std::logic_error(
			"pg::QueryBuilder: '"
			+ std::string(condition) + "' has "
			+ std::to_string(next - first) + " '?' but "
			+ std::to_string(count) + " values"
		);
	// Entre parentheses : << a = $1 OR b = $2 >> reste un seul bloc face aux AND
	this->_conditions.push_back("(" + numbered + ")");
}

pg::QueryBuilder &pg::QueryBuilder::orderBy(
	std::string_view key,
	const SortWhitelist &allowed

)
{
	const auto found = allowed.find(key);
	if (found == allowed.end())
		this->_error = "unknown sort key '" + std::string(key) + "'";
	else
		this->_orderBy = found->second;
	return *this;
}

pg::QueryBuilder &pg::QueryBuilder::limit(std::int64_t count)
{
	this->_params.add(count);
	this->_limitParam = this->_params.size();
	return *this;
}

core::Result<pg::Query> pg::QueryBuilder::build() const
{
	if (this->_error)
		return core::fail(core::ErrorCode::Invalid, *this->_error);
	std::string sql = this->_select;
	for (std::size_t i = 0; i < this->_conditions.size(); ++i)
		sql += (i == 0 ? " WHERE " : " AND ") + this->_conditions[i];
	if (!this->_orderBy.empty())
		sql += " ORDER BY " + this->_orderBy;
	if (this->_limitParam)
		sql += " LIMIT $" + std::to_string(*this->_limitParam);
	return Query{std::move(sql), this->_params};
}