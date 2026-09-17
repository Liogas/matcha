#include <gtest/gtest.h>
#include <auth/AuthService.hpp>
#include "FakeUserRepository.hpp"

TEST(AuthServiceTest, CanBeCreated)
{
	FakeUserRepository repository;
	auth::AuthService service(repository);
}