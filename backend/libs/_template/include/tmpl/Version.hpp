#pragma once
#include <string_view>

namespace tmpl
{
	// Version de la bibliotheque, au format MAJEUR.MINEUR.CORRECTIF
	std::string_view	version() noexcept;
}