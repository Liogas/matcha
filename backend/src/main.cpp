#include <crow.h>
#include <crow/middlewares/cors.h>
#include <libpq-fe.h>
#include "database/Database.hpp"
#include "database/DatabaseInitializer.hpp"
#include "repositories/UserRepository.hpp"
#include "security/PasswordHasher.hpp"
#include "http/AuthRoutes.hpp"
#include <iostream>

int main()
{
	Database database;
	DatabaseInitializer databaseInitializer(database);
    if (!databaseInitializer.run())
    {
        std::cerr << "[ERROR BDD] Database initialization failed" << std::endl;
        return 1;
    }
	UserRepository userRepository(database);
	auth::PasswordHasher passwordHasher;
	auth::AuthService authService(
		userRepository,
		passwordHasher
	);
    crow::SimpleApp app;

    // auto& cors = app.get_middleware<crow::CORSHandler>();

    // cors.global()
    //     .origin("http://localhost:5173");

	registerAuthRoutes(app, authService);

    app.port(18080).multithreaded().run();
}