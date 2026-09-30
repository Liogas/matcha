#include <gtest/gtest.h>

#include <auth/http/HttpClient.hpp>
#include <auth/http/CppHttpClient.hpp>

#include <httplib.h>
#include <thread>

namespace
{
    class FakeHttpClient : public auth::HttpClient
    {
    public:
        auth::HttpResult result{
            auth::HttpResult::Status::NetworkError,
            std::nullopt
        };

        bool called = false;
        std::string lastUrl;

        auth::HttpResult get(const std::string& url) override
        {
            called = true;
            lastUrl = url;
            return result;
        }
    };
}

TEST(HttpClientTests, ReturnsSuccessfulResponse)
{
    FakeHttpClient client;

    client.result = {
        auth::HttpResult::Status::Success,
        auth::HttpResponse{
            200,
            R"({"message":"ok"})"
        }
    };

    const auto result = client.get("https://example.com/test");

    ASSERT_EQ(
        result.status,
        auth::HttpResult::Status::Success
    );

    ASSERT_TRUE(result.response.has_value());

    EXPECT_EQ(result.response->statusCode, 200);
    EXPECT_EQ(result.response->body, R"({"message":"ok"})");

    EXPECT_TRUE(client.called);
    EXPECT_EQ(client.lastUrl, "https://example.com/test");
}

TEST(HttpClientTests, ReturnsNetworkError)
{
    FakeHttpClient client;

    client.result = {
        auth::HttpResult::Status::NetworkError,
        std::nullopt
    };

    const auto result = client.get("https://example.com/test");

    EXPECT_EQ(
        result.status,
        auth::HttpResult::Status::NetworkError
    );

    EXPECT_FALSE(result.response.has_value());

    EXPECT_TRUE(client.called);
    EXPECT_EQ(client.lastUrl, "https://example.com/test");
}

TEST(CppHttpClientTests, GetsHttpResponse)
{
    httplib::Server server;

    server.Get(
        "/test",
        [](const httplib::Request&, httplib::Response& response)
        {
            response.status = 200;
            response.set_content(
                "hello",
                "text/plain"
            );
        }
    );

    const int port =
        server.bind_to_any_port("127.0.0.1");

    ASSERT_GT(port, 0);

    std::thread serverThread(
        [&server]()
        {
            server.listen_after_bind();
        }
    );

    std::cout << "SERVER STARTED ON PORT "
              << port
              << std::endl;

    // --------------------------------------------------
    // Test de notre CppHttpClient
    // --------------------------------------------------

    auth::CppHttpClient client;

    const std::string url =
        "http://127.0.0.1:" +
        std::to_string(port) +
        "/test";

    std::cout << "URL = [" << url << "]"
              << std::endl;

    const auto result =
        client.get(url);

    std::cout << "CPP-AUTH GET FINISHED"
              << std::endl;

    // --------------------------------------------------
    // Arrêt du serveur
    // --------------------------------------------------

    server.stop();
    serverThread.join();

    // --------------------------------------------------
    // Vérifications
    // --------------------------------------------------

    ASSERT_EQ(
        result.status,
        auth::HttpResult::Status::Success
    );

    ASSERT_TRUE(
        result.response.has_value()
    );

    EXPECT_EQ(
        result.response->statusCode,
        200
    );

    EXPECT_EQ(
        result.response->body,
        "hello"
    );
}