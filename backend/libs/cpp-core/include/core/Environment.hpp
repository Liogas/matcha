#pragma once
#include <map>
#include <optional>
#include <string>

namespace core
{
	// D'où viennent les variables d'environnement : le vrai environnement du
	// processus, ou une table en mémoire dans les tests.
	class IEnvironment
	{
		public:
			virtual ~IEnvironment() = default;
			virtual std::optional<std::string>	get(const std::string &name) const = 0;
	};

	class SystemEnvironment : public IEnvironment
	{
		public:
			std::optional<std::string>	get(const std::string &name) const override;
	};

	class MapEnvironment : public IEnvironment
	{
		public:
			void						set(const std::string &name, std::string value);
			std::optional<std::string>	get(const std::string &name) const override;

		private:
			std::map<std::string, std::string>	_values;
	};
}