#include <pg/Params.hpp>

std::size_t	pg::Params::size() const
{
	return this->_values.size();
}

const std::vector<pg::Text>	&pg::Params::values() const
{
	return this->_values;
}