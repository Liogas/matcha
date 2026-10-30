#include <pg/Migrations.hpp>
#include <pg/Pool.hpp>
#include <pg/QueryBuilder.hpp>

// Compile avec les en-têtes installés, et se lie à libpq : Connection::open
// vers un port fermé doit échouer proprement.
int main()
{
	pg::ConnectionSettings settings;
	settings.host = "127.0.0.1";
	settings.port = "1";
	const auto connection = pg::Connection::open(settings);
	const bool unavailable = !connection && connection.error().code == core::ErrorCode::Unavailable;

	pg::QueryBuilder builder("SELECT 1");
	builder.where("1 = ?", 1);
	const auto query = builder.build();
	return (unavailable && query && query->params.size() == 1) ? 0 : 1;
}