#pragma once
#include <core/Error.hpp>
#include <string_view>

namespace pg
{
	// Traduit un code SQLSTATE de PostgreSQL (5 caractères, ex. "23505") en
	// catégorie d'erreur de cpp-core.
	core::ErrorCode	errorCodeFor(std::string_view sqlState) noexcept;
}