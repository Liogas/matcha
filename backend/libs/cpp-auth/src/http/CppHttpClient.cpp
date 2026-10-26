#include <auth/http/CppHttpClient.hpp>
#include <httplib.h>
#include <auth/http/UrlParser.hpp>
#include <iostream>
#include <stdexcept>

namespace auth
{
	CppHttpClient::CppHttpClient(
		const HttpClientConfig &config
	):
		_config(config)
	{
		if (this->_config.connectionTimeout <=
				std::chrono::milliseconds::zero()
			|| this->_config.readTimeout <=
				std::chrono::milliseconds::zero()
			|| this->_config.writeTimeout <=
				std::chrono::milliseconds::zero()
			|| this->_config.maxResponseSize == 0)
			throw std::invalid_argument(
				"HTTP timeouts and maximum response size must be positive"
			);
	}

	HttpResult	CppHttpClient::get(
		const std::string &url
	)
	{
		const auto parsedUrl = UrlParser::parse(url);
		if (!parsedUrl.has_value())
			return {
				HttpResult::Status::NetworkError,
				std::nullopt
			};
		if (parsedUrl->scheme == "https")
		{
			httplib::SSLClient client(
				parsedUrl->host,
				parsedUrl->port.empty() ? 
					443 : std::stoi(parsedUrl->port)
			);
			client.set_connection_timeout(this->_config.connectionTimeout);
			client.set_read_timeout(this->_config.readTimeout);
			client.set_write_timeout(this->_config.writeTimeout);
			client.set_payload_max_length(this->_config.maxResponseSize);
			client.enable_server_certificate_verification(true);
			if (this->_config.caCertPath.has_value())
				client.set_ca_cert_path(
					this->_config.caCertPath->c_str()
				);
			const auto response = client.Get(parsedUrl->path);
			if (!response)
			{
				std::cerr	<< "[HTTPS ERROR] "
							<< httplib::to_string(response.error()) << std::endl;
				return {
					HttpResult::Status::NetworkError,
					std::nullopt
				};
			}
			return {
				HttpResult::Status::Success,
				HttpResponse{
					response->status,
					response->body
				}
			};
		} else if (parsedUrl->scheme == "http")
		{
			if (!this->_config.allowInsecureHttp)
				return {
					HttpResult::Status::NetworkError,
					std::nullopt
				};
			httplib::Client client(
				parsedUrl->host,
				parsedUrl->port.empty() 
					? 80 : std::stoi(parsedUrl->port)
			);
			client.set_connection_timeout(this->_config.connectionTimeout);
			client.set_read_timeout(this->_config.readTimeout);
			client.set_write_timeout(this->_config.writeTimeout);
			client.set_payload_max_length(this->_config.maxResponseSize);
			const auto response = client.Get(parsedUrl->path);
			if (!response)
				return {
					HttpResult::Status::NetworkError,
					std::nullopt
				};
			return {
				HttpResult::Status::Success,
				HttpResponse{
					response->status,
					response->body
				}
			};
		}
		return {
			HttpResult::Status::NetworkError,
			std::nullopt
		};
	}
}