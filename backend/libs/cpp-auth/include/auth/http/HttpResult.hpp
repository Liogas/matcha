#pragma once

#include <optional>
#include "HttpResponse.hpp"

namespace	auth
{
	struct HttpResult
	{
		enum class Status
		{
			Success,
			NetworkError
		};
		Status status;
		std::optional<HttpResponse> response;
	};
}