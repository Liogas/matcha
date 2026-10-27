#pragma once
#include <core/Environment.hpp>
#include <core/Result.hpp>
#include <string>
#include <vector>

namespace core
{
	// Lit la configuration au démarrage. Une variable absente ou invalide ne
	// lève pas d'exception : l'erreur est notée, une valeur neutre est renvoyée,
	// et finish() les rapporte toutes ensemble.
	class ConfigReader
	{
	public:
		explicit ConfigReader(const IEnvironment &environment);

		std::string required(const std::string &name);
		std::string optional(const std::string &name, const std::string &fallback);
		int requiredInt(const std::string &name);
		bool optionalBool(const std::string &name, bool fallback);
		// Lit NAME, ou le contenu du fichier désigné par NAME_FILE (secrets Docker).
		std::string requiredSecret(const std::string &name);

		Result<void> finish() const;

	private:
		const IEnvironment &_environment;
		std::vector<std::string> _errors;
	};
}