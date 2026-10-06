#include <auth/auth/Authenticator.hpp>

namespace auth
{
	Authenticator::Authenticator(
		TokenValidatorInterface &validator
	): _validator(validator)
	{}

	TokenValidationResult	Authenticator::authenticate(
		const std::string &token
	)
	{
		return (this->_validator.validate(token));
	}
}