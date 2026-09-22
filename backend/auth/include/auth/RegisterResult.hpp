#pragma once

namespace auth
{
	enum class RegisterResult
	{
		Success,
		EmailAlreadyExists,
		InvalidEmail,
		InvalidPassword,
		InternalError
	};
}