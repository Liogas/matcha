#pragma once
#include <pg/Convert.hpp>
#include <cstddef>
#include <vector>

namespace pg
{
	// Les valeurs de $1, $2… d'une requête, déjà converties en texte.
	class Params
	{
	public:
		template <class T>
		void add(const T &value)
		{
			this->_values.push_back(toText(value));
		}

		std::size_t size() const;
		const std::vector<Text> &values() const;

	private:
		std::vector<Text> _values;
	};

	// pg::params(42, "alice", std::optional<int>{}) : $1 = 42, $2 = alice, $3 = NULL
	template <class... Args>
	Params params(const Args &...args)
	{
		Params result;
		(result.add(args), ...); // fold expression : un add par argument, dans l'ordre
		return result;
	}
}