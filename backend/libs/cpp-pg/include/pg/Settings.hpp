#pragma once
#include <core/ConfigReader.hpp>
#include <string>

namespace pg
{
	// Paramètres de connexion. Ils sont passés à libpq un par un
	// (PQconnectdbParams) : rien à échapper, même si le mot de passe contient
	// une espace ou une apostrophe.
	struct ConnectionSettings
	{
		std::string	host;
		std::string	port = "5432";
		std::string	database;
		std::string	user;
		std::string	password;
		std::string	applicationName = "cpp-pg";
		int			connectTimeoutSeconds = 5;
	};

	// Lit <prefix>HOST, <prefix>PORT, <prefix>NAME, <prefix>USER et
	// <prefix>PASSWORD (ou <prefix>PASSWORD_FILE). Les erreurs sont notées
	// dans config, comme pour toute lecture : à vérifier avec config.finish().
	ConnectionSettings	readSettings(core::ConfigReader &config, const std::string &prefix);
}