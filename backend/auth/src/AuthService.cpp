#include <auth/AuthService.hpp>

namespace auth
{
	AuthService::AuthService(
		IUserRepository &repo,
		IPasswordHasher	&pwdHasher
	):
		_userRepository(repo),
		_pwdHasher(pwdHasher)
	{}

	bool	AuthService::userExists(const std::string &email) const
	{
		return (this->_userRepository).findByEmail(email).has_value();
	}

	RegisterResult	AuthService::registerUser(
		const std::string &email,
		const std::string &password
	)
	{
		if (!isValidEmail(email))
			return RegisterResult::InvalidEmail;
		if (this->_userRepository.findByEmail(email).has_value())
			return RegisterResult::EmailAlreadyExists;
		const std::string pwdHash = this->_pwdHasher.hash(password);
		const auto userId = this->_userRepository.createUser(
			email,
			pwdHash
		);
		if (!userId.has_value())
			return RegisterResult::CreationFailed;
		return RegisterResult::Success;
	}

	bool	AuthService::isValidEmail(const std::string &email) const
	{
		const std::size_t atPosition = email.find('@');
		if (atPosition == std::string::npos)
			return (false);
		if (atPosition == 0)
			return (false);
		const std::size_t dotPosition = email.find('.', atPosition);
		if (dotPosition == std::string::npos)
			return (false);
		if (dotPosition == atPosition + 1)
			return (false);
		if (dotPosition == email.size() - 1)
			return (false);
		return (true);
	}
}
