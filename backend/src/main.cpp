#include <crow.h>
#include <crow/middlewares/cors.h>

int main()
{
    crow::App<crow::CORSHandler> app;

    auto& cors = app.get_middleware<crow::CORSHandler>();

    cors.global()
        .origin("http://localhost:5173");

    CROW_ROUTE(app, "/")
    ([] {
        return "Gaston\n";
    });

	CROW_ROUTE(app, "/api/profil/")
	.methods(crow::HTTPMethod::GET, crow::HTTPMethod::PATCH)
	([] {
		crow::json::wvalue data;
		data["name"] = "Gaston";
		data["city"] = "Paris";
		data["age"] = 27;
		data["bio"] = "Je suis une description";
		return (data);
	});

	CROW_ROUTE(app, "/api/profil/")
	.methods(crow::HTTPMethod::POST)
	([](const crow::request& req)
	{
		auto body = crow::json::load(req.body);
		if (!body)
			return crow::response(400, "Invalid JSON");
		std::string name = body["name"].s();
		int age = body["age"].i();
		std::string city = body["city"].s();
		std::string bio = body["bio"].s();

		std::cout << "Name : " << name << std::endl;
		std::cout << "Age : " << age << std::endl;
		std::cout << "City : " << city << std::endl;
		std::cout << "Bio : " << bio << std::endl;

		return crow::response(201, "Datas modified");
	});

    app.port(18080).multithreaded().run();
}