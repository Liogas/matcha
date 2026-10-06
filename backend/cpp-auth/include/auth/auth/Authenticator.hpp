#pragma once

#include <auth/token/TokenValidatorInterface.hpp>

namespace auth
{
	class Authenticator
	{
		public:
			explicit Authenticator(
				TokenValidatorInterface &validator
			);

			TokenValidationResult	authenticate(
				const std::string &token
			);
		private:
			TokenValidatorInterface &_validator;
	};
}