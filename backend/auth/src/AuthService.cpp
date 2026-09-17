#include <auth/AuthService.hpp>

namespace auth
{
	AuthService::AuthService(IUserRepository &repo):
		_userRepository(repo)
	{}
}