#include <pg/Settings.hpp>

pg::ConnectionSettings	pg::readSettings(core::ConfigReader &config, const std::string &prefix)
{
	ConnectionSettings settings;
	settings.host = config.required(prefix + "HOST");
	settings.port = std::to_string(config.requiredInt(prefix + "PORT"));
	settings.database = config.required(prefix + "NAME");
	settings.user = config.required(prefix + "USER");
	settings.password = config.requiredSecret(prefix + "PASSWORD");
	return settings;
}