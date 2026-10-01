#include <gtest/gtest.h>

#include <auth/http/HttpClient.hpp>
#include <auth/http/CppHttpClient.hpp>

#include <httplib.h>
#include <thread>
#include <filesystem>
#include <chrono>

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

TEST(CppHttpClientTests, GetsHttpsResponse)
{
    const std::string certPath =
        "cpp-auth/tests/certs/test-cert.pem";

    const std::string keyPath =
        "cpp-auth/tests/certs/test-key.pem";

    std::cout << "CERT PATH = ["
            << certPath
            << "]"
            << std::endl;

    std::cout << "KEY PATH = ["
            << keyPath
            << "]"
            << std::endl;

    std::cout << "CWD = ["
            << std::filesystem::current_path()
            << "]"
            << std::endl;

    std::cout << "CERT EXISTS = "
            << std::filesystem::exists(certPath)
            << std::endl;

    std::cout << "KEY EXISTS = "
            << std::filesystem::exists(keyPath)
            << std::endl;

    httplib::SSLServer server(
        certPath.c_str(),
        keyPath.c_str()
    );

    ASSERT_TRUE(server.is_valid());

    server.Get(
        "/test",
        [](const httplib::Request&, httplib::Response& response)
        {
            response.status = 200;
            response.set_content(
                "hello https",
                "text/plain"
            );
        }
    );

    const int port =
        server.bind_to_any_port("127.0.0.1");

    ASSERT_GT(port, 0);

    std::cout << "[PORT] " << port << std::endl;

    std::thread serverThread(
        [&server]()
        {
            server.listen_after_bind();
        }
    );
    
    std::cout
        << "[SERVER IS_RUNNING] "
        << server.is_running()
        << std::endl;

    std::this_thread::sleep_for(
        std::chrono::seconds(60)
    );

    std::cout << "[DIRECT BASE URL] [https://127.0.0.1:"
          << port
          << "]"
          << std::endl;

    httplib::SSLClient directClient(
        "https://127.0.0.1:" + std::to_string(port)
    );

    directClient.enable_server_certificate_verification(false);

    const auto directResponse =
        directClient.Get("/test");

    if (!directResponse)
    {
        std::cout
            << "[DIRECT HTTPS ERROR] "
            << httplib::to_string(directResponse.error())
            << std::endl;
    }
    else
    {
        std::cout
            << "[DIRECT HTTPS SUCCESS] "
            << directResponse->status
            << " / "
            << directResponse->body
            << std::endl;
    }

    auth::CppHttpClient client({
        "../../cpp-auth/tests/certs/test-cert.pem"
    });

    const std::string url =
        "https://127.0.0.1:" +
        std::to_string(port) +
        "/test";

    const auto result =
        client.get(url);

    server.stop();
    serverThread.join();

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
        "hello https"
    );
}