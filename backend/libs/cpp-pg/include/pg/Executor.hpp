#pragma once
#include <core/Result.hpp>
#include <pg/Params.hpp>
#include <pg/Rows.hpp>
#include <string_view>

namespace pg
{
	// Ce qui sait exécuter une requête : une Connection, une Transaction, ou le
	// ConnectionPool (qui emprunte une connexion le temps de la requête).
	// Un repository reçoit un Executor & : il ne sait pas, et n'a pas à savoir,
	// s'il travaille dans une transaction ou non.
	class Executor
	{
	public:
		// Une seule instruction SQL. Les valeurs passent en $1, $2…
		virtual core::Result<Rows> execute(std::string_view sql, const Params &params = {}) = 0;

		// executor.query("SELECT ... WHERE id = $1", id)
		template <class... Args>
		core::Result<Rows> query(std::string_view sql, const Args &...args)
		{
			return this->execute(sql, pg::params(args...));
		}

	protected:
		// Protégé et non virtuel : on ne détruit jamais un objet à travers un
		// Executor *. Les copies et déplacements restent ceux des classes filles.
		Executor() = default;
		Executor(const Executor &) = default;
		Executor(Executor &&) = default;
		Executor &operator=(const Executor &) = default;
		Executor &operator=(Executor &&) = default;
		~Executor() = default;
	};
}