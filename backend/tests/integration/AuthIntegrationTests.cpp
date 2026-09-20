#include <gtest/gtest.h>
#include <auth/AuthService.hpp>
#include <auth/tests/FakePasswordHasher.hpp>

#include "database/Database.hpp"
#include "repositories/UserRepository.hpp"

// TEST 1
TEST(AuthIntegrationTest, CanRegisterUser)
{
	Database database;
	UserRepository repository;
	FakePasswordHasher pwdHasher;

	auth::AuthService authService(repository, pwdHasher);
	const std::string email = "auth_integration@example.com";
	const std::string pwd = "test123";
	database.executeParams(
		"DELETE FROM users WHERE email = $1",
		{email}
	);
	const auto result = authService.registerUser(
		email,
		password
	);
	ASSERT_EQ(result, auth::RegisterResult::Success);
	const auto user = repository.findByEmail(email);
	ASSERT_TRUE(user.has_value());
	EXPECT_EQ(user->email, email);
	EXPECT_EQ(user->passwordHash, "hash_test123");
	database.executeParams(
		"DELETE FROM users WHERE email = $1",
		{email}
	);
}

// TEST 2
TEST(AuthIntegrationTest, CannotRegisterExistingEmail)
{
	Database database;
	UserRepository repository(database);
	FakePasswordHasher pwdHasher;
	auth::AuthService authService(
		repository,
		pwdHasher
	);
	const std::string email = "existing@example.com";
	const std::string password = "my_password";
	database.executeParams(
		"DELETE FROM users WHERE email = $1",
		{email}
	);
	const auto firstResult = authService.registerUser(
		email,
		password
	);
	ASSERT_EQ(firstResult, auth::RegisterResult::Success);
	const auto secondResult = authService.registerUser(
		email,
		password
	);
	EXPECT_EQ(secondResult, auth::RegisterResult::EmailAlreadyExists);
	database.executeParams(
		"DELETE FROM users WHERE email = $1",
		{email}
	);
}
