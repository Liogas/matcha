#pragma once
#include <core/Result.hpp>
#include <pg/Params.hpp>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pg
{
	// Une requête prête à partir : executor.execute(query.sql, query.params).
	struct Query
	{
		std::string sql;
		Params params;
	};

	// Les tris qu'un appelant a le droit de demander : une clé publique (venue
	// de l'URL, donc d'un utilisateur) vers un ORDER BY écrit par nous.
	// std::less<> : on cherche directement avec un std::string_view.
	using SortWhitelist = std::map<std::string, std::string, std::less<>>;

	// Construit une requête SELECT dont les conditions dépendent des filtres.
	// Les valeurs passent toujours en paramètres : chaque « ? » d'une condition
	// devient $1, $2… dans l'ordre. Ce n'est pas un ORM : le SQL reste écrit à
	// la main, le builder ne fait que l'assembler.
	//
	//   pg::QueryBuilder query("SELECT id FROM profiles p");
	//   query.where("p.age >= ?", 18).whereIf(city, "p.city = ?").limit(20);
	//   auto built = query.build();   // WHERE p.age >= $1 AND p.city = $2 LIMIT $3
	class QueryBuilder
	{
	public:
		// Le début de la requête : SELECT … FROM … (et ses JOIN).
		explicit QueryBuilder(std::string select);

		// Ajoute une condition (reliée aux autres par AND). Autant d'arguments
		// que de « ? » : sinon c'est un bug, et std::logic_error.
		template <class... Args>
		QueryBuilder &where(std::string_view condition, const Args &...args)
		{
			const std::size_t first = this->_params.size() + 1;
			(this->_params.add(args), ...);
			this->addCondition(condition, first, sizeof...(Args));
			return *this;
		}

		// La condition n'est ajoutée que si value contient une valeur, qui
		// remplace l'unique « ? ». whereIf(minAge, "p.age >= ?")
		template <class T>
		QueryBuilder &whereIf(const std::optional<T> &value, std::string_view condition)
		{
			if (value)
				this->where(condition, *value);
			return *this;
		}

		// Le tri demandé par key, s'il est dans allowed. Une clé inconnue n'est
		// pas un bug (elle vient d'une URL) : build() renverra une erreur Invalid.
		QueryBuilder &orderBy(std::string_view key, const SortWhitelist &allowed);

		QueryBuilder &limit(std::int64_t count);

		core::Result<Query> build() const;

	private:
		void addCondition(std::string_view condition, std::size_t first, std::size_t count);

		std::string _select;
		std::vector<std::string> _conditions;
		std::string _orderBy;
		std::optional<std::string> _error;
		std::optional<std::size_t> _limitParam;
		Params _params;
	};
}