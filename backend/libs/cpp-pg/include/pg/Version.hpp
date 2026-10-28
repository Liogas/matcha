#pragma once
#include <string_view>

namespace pg
{
	// Version de la bibliotheque, au format MAJEUR.MINEUR.CORRECTIF
	std::string_view	version() noexcept;
}