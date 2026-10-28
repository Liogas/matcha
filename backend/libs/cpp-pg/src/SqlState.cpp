#include <pg/SqlState.hpp>

core::ErrorCode pg::errorCodeFor(std::string_view sqlState) noexcept
{
	if (sqlState == "23505") // unique_violation : déjà pris
		return core::ErrorCode::Conflict;
	if (sqlState.starts_with("23")	   // autres contraintes (FK, CHECK, NOT NULL)
		|| sqlState.starts_with("22")) // donnée invalide (texte trop long, "abc" en entier…)
		return core::ErrorCode::Invalid;
	if (sqlState == "40001"			  // serialization_failure : à réessayer
		|| sqlState == "40P01"		  // deadlock_detected : à réessayer
		|| sqlState.starts_with("08") // connexion perdue
		|| sqlState == "53300"		  // too_many_connections
		|| sqlState == "57P01")		  // serveur arrêté par l'administrateur
		return core::ErrorCode::Unavailable;
	return core::ErrorCode::Internal; // syntaxe, table inconnue… : un bug
}