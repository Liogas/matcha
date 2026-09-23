#include <auth/AuthService.hpp>
#include <cctype>

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
		const auto result = this->_userRepository.findByEmail(email);
		return result.status == UserLookupResult::Status::Found;
	}

	RegisterResult	AuthService::registerUser(
		const std::string &email,
		const std::string &password
	)
	{
		if (!isValidEmail(email))
			return RegisterResult::InvalidEmail;
		if (!isValidPwd(password))
			return RegisterResult::InvalidPassword;
		const auto result = this->_userRepository.findByEmail(email);
		if (result.status == UserLookupResult::Status::Found)
			return RegisterResult::EmailAlreadyExists;
		if (result.status == UserLookupResult::Status::DatabaseError)
			return RegisterResult::InternalError;
		std::string pwdHash;
		try
		{
			pwdHash = this->_pwdHasher.hash(password);
		}
		catch(const std::exception& e)
		{
			return RegisterResult::InternalError;
		}
		const auto createResult = this->_userRepository.createUser(
			email,
			pwdHash
		);

		if (createResult.status == CreateUserResult::Status::DatabaseError)
			return RegisterResult::InternalError;
		if (createResult.status == CreateUserResult::Status::EmailAlreadyExists)
			return RegisterResult::EmailAlreadyExists;
		else if (createResult.status != CreateUserResult::Status::Success)
			return RegisterResult::InvalidEmail;
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

	LoginResult	AuthService::login(
		const std::string &email,
		const std::string &pwd
	)
	{
		const auto result = this->_userRepository.findByEmail(email);
		if (result.status == UserLookupResult::Status::DatabaseError)
			return {
				LoginResult::Status::InternalError,
				std::nullopt
			};
		if (result.status == UserLookupResult::Status::NotFound)
			return {
				LoginResult::Status::InvalidCredentials,
				std::nullopt
			};
		if (!this->_pwdHasher.verify(
			pwd,
			result.user->passwordHash
		))
		{
			return {
				LoginResult::Status::InvalidCredentials,
				std::nullopt
			};
		}
		return {
			LoginResult::Status::Success,
			AuthenticatedUser{
				result.user->id,
				result.user->email
			}
		};
	}

	bool	AuthService::isValidPwd(const std::string &pwd) const
	{
		if (pwd.size() < 8)
			return false;
		bool hasUpperCase = false;
		bool hasLowerCase = false;
		bool hasDigit = false;
		bool hasSpecial = false;

		for (const char c : pwd)
		{
			if (std::isupper(static_cast<unsigned char>(c)))
				hasUpperCase = true;
			else if (std::islower(static_cast<unsigned char>(c)))
				hasLowerCase = true;
			else if (std::isdigit(static_cast<unsigned char>(c)))
				hasDigit = true;
			else
				hasSpecial = true;
		}
		return hasUpperCase && hasSpecial && hasLowerCase && hasDigit;
	}
}
