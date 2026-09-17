#pragma once

#include "IUserRepository.hpp"

namespace auth
{
	class AuthService
	{
		public:
			explicit AuthService(IUserRepository &repo);
		private:
			IUserRepository	&_userRepository;
	};
}