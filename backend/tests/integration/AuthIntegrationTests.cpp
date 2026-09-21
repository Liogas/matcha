#include <gtest/gtest.h>
#include <auth/AuthService.hpp>
#include "security/PasswordHasher.hpp"

#include "database/Database.hpp"
#include "repositories/UserRepository.hpp"

// TEST 1
TEST(AuthIntegrationTest, CanRegisterUser)
{
	Database database;
	UserRepository repository(database);
	auth::PasswordHasher pwdHasher;

	auth::AuthService authService(repository, pwdHasher);
	const std::string email = "auth_integration@example.com";
	const std::string pwd = "test123";
	database.executeParams(
		"DELETE FROM users WHERE email = $1",
		{email}
	);
	const auto result = authService.registerUser(
		email,
		pwd
	);
	ASSERT_EQ(result, auth::RegisterResult::Success);
	const auto userResult = repository.findByEmail(email);
	EXPECT_EQ(userResult.status, auth::UserLookupResult::Status::Found);
	ASSERT_TRUE(userResult.user.has_value());
	EXPECT_EQ(userResult.user->email, email);
	EXPECT_NE(userResult.user->passwordHash, pwd);
	EXPECT_FALSE(userResult.user->passwordHash.empty());
	EXPECT_TRUE(
		pwdHasher.verify(
			pwd,
			userResult.user->passwordHash
		)
	);
	EXPECT_FALSE(
		pwdHasher.verify(
			"wrong_password",
			userResult.user->passwordHash
		)
	);
	database.executeParams(
		"DELETE FROM users WHERE email = $1",
		{email}
	);
}

// TEST 2
// TEST(AuthIntegrationTest, CannotRegisterExistingEmail)
// {
// 	Database database;
// 	UserRepository repository(database);
// 	FakePasswordHasher pwdHasher;
// 	auth::AuthService authService(
// 		repository,
// 		pwdHasher
// 	);
// 	const std::string email = "existing@example.com";
// 	const std::string password = "my_password";
// 	database.executeParams(
// 		"DELETE FROM users WHERE email = $1",
// 		{email}
// 	);
// 	const auto firstResult = authService.registerUser(
// 		email,
// 		password
// 	);
// 	ASSERT_EQ(firstResult, auth::RegisterResult::Success);
// 	const auto secondResult = authService.registerUser(
// 		email,
// 		password
// 	);
// 	EXPECT_EQ(secondResult, auth::RegisterResult::EmailAlreadyExists);
// 	database.executeParams(
// 		"DELETE FROM users WHERE email = $1",
// 		{email}
// 	);
// }
