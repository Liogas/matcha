#pragma once
#include <pg/Convert.hpp>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

// Le type de libpq (PGresult), déclaré sans inclure libpq-fe.h.
struct pg_result;

namespace pg
{
	// Erreur de programmation en lisant une ligne : colonne inconnue, NULL lu
	// dans un type non optionnel, texte illisible dans le type demandé.
	// C'est un bug à corriger, pas une erreur attendue : exception.
	class RowError : public std::logic_error
	{
	public:
		using std::logic_error::logic_error;
	};

	namespace detail
	{
		template <class T>
		struct IsOptional : std::false_type
		{
		};
		template <class T>
		struct IsOptional<std::optional<T>> : std::true_type
		{
		};
	}

	// Une ligne d'un résultat. Garde le résultat en vie (shared_ptr) : une Row
	// reste valide même après la destruction de ses Rows.
	class Row
	{
	public:
		// row.get<std::int64_t>("id"), row.get<std::optional<std::string>>("bio")
		template <class T>
		T get(std::string_view column) const;

	private:
		friend class Rows;
		Row(std::shared_ptr<pg_result> result, int index);

		// nullptr si la valeur est NULL. RowError si la colonne n'existe pas.
		const char *text(std::string_view column) const;

		std::shared_ptr<pg_result> _result;
		int _index;
	};

	// Le résultat d'une requête : des lignes (SELECT, RETURNING) ou rien.
	class Rows
	{
	public:
		class Iterator
		{
		public:
			Row operator*() const;
			Iterator &operator++();
			bool operator==(const Iterator &other) const = default;

		private:
			friend class Rows;
			Iterator(const Rows *rows, std::size_t index);

			const Rows *_rows;
			std::size_t _index;
		};

		std::size_t size() const;
		bool empty() const;
		Row operator[](std::size_t index) const; // RowError si hors limites
		Iterator begin() const;
		Iterator end() const;

		// Lignes touchées par INSERT, UPDATE ou DELETE.
		std::int64_t affected() const;
		// La réponse du serveur : "INSERT 0 1", "COMMIT", "ROLLBACK"…
		std::string_view command() const;

	private:
		friend class Connection;
		explicit Rows(pg_result *result); // en prend possession

		std::shared_ptr<pg_result> _result;
	};

	template <class T>
	T Row::get(std::string_view column) const
	{
		const char *value = this->text(column);
		if constexpr (detail::IsOptional<T>::value)
		{
			if (value == nullptr)
				return std::nullopt;
			typename T::value_type out{};
			if (!fromText(value, out))
				throw RowError("column '" + std::string(column) + "': cannot read '" + value + "'");
			return out;
		}
		else
		{
			if (value == nullptr)
				throw RowError("column '" + std::string(column) + "' is NULL");
			T out{};
			if (!fromText(value, out))
				throw RowError("column '" + std::string(column) + "': cannot read '" + value + "'");
			return out;
		}
	}
}