#include <gtest/gtest.h>
#include <auth/credentials/AuthService.hpp>
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
	const std::string pwd = "Abcdefg1!";
	database.executeParams(
		"DELETE FROM users WHERE email = $1",
		{email}
	);
	const auto result = authService.registerUser(
		email,
		pwd
	);
	std::cout << "CA MARCHE PAS JUSTE APRES : " << std::endl;
	if (result == auth::RegisterResult::EmailAlreadyExists)
		std::cout << "1";
	else if (result == auth::RegisterResult::InternalError)
		std::cout << "2";
	else if (result == auth::RegisterResult::InvalidEmail)
		std::cout << "3";
	else if (result == auth::RegisterResult::InvalidPassword)
		std::cout << "4";
	else if (result == auth::RegisterResult::Success)
		std::cout << "5";
	std::cout << std::endl;
	ASSERT_EQ(result, auth::RegisterResult::Success);
	std::cout << "!!!!!!!!!!!!!!!!! Je suis ici 3 !!!!!!!!!!!!!!!!!" << std::endl;
	const auto userResult = repository.findByEmail(email);
	EXPECT_EQ(userResult.status, auth::UserLookupResult::Status::Found);
	std::cout << "!!!!!!!!!!!!!!!!! Je suis ici 1 !!!!!!!!!!!!!!!!!" << std::endl;
	ASSERT_TRUE(userResult.user.has_value());
	EXPECT_EQ(userResult.user->email, email);
	EXPECT_NE(userResult.user->passwordHash, pwd);
	EXPECT_FALSE(userResult.user->passwordHash.empty());
	std::cout << "!!!!!!!!!!!!!!!!! Je suis ici 2 !!!!!!!!!!!!!!!!!" << std::endl;
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
