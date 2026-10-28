#include <pg/Pool.hpp>

// Compile avec les en-têtes installés, et se lie à libpq : Connection::open
// vers un port fermé doit échouer proprement.
int main()
{
	pg::ConnectionSettings settings;
	settings.host = "127.0.0.1";
	settings.port = "1";
	const auto connection = pg::Connection::open(settings);
	const bool unavailable = !connection
		&& connection.error().code == core::ErrorCode::Unavailable;
	return (unavailable && pg::params(1, "a").size() == 2) ? 0 : 1;
}