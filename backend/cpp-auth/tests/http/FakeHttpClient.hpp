#pragma once

#include <auth/http/HttpClient.hpp>

class FakeHttpClient : public auth::HttpClient
{
	public:
		explicit FakeHttpClient(auth::HttpResult result)
			: _result(std::move(result))
		{
		}

		auth::HttpResult get(const std::string& url) override
		{
			return _result;
		}

	private:
		auth::HttpResult _result;
};