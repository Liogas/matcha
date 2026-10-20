#pragma once

#include "IUserRepository.hpp"
#include "IUserLookupRepository.hpp"
#include "IPasswordHasher.hpp"
#include "RegisterResult.hpp"
#include "LoginResult.hpp"

#include <string>

namespace auth
{
	class AuthService
	{
		public:
			explicit AuthService(
				IUserRepository &repo,
				IPasswordHasher	&pwdHasher
			);
			explicit AuthService(
				IUserLookupRepository &repo,
				IPasswordHasher &pwdHasher
			);
			bool	userExists(const std::string &email) const;
			RegisterResult	registerUser(
				const std::string &email,
				const std::string &pwd
			);
			LoginResult		login(
				const std::string &email,
				const std::string &pwd
			);
			bool	isValidPwd(const std::string &pwd) const;
		private:
			IUserLookupRepository &_userLookupRepository;
			IUserRepository *_userRepository;
			IPasswordHasher	&_pwdHasher;
			bool	isValidEmail(const std::string &email) const;
	};
}