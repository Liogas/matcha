#include <gtest/gtest.h>
#include <auth/credentials/AuthService.hpp>
#include "FakeUserRepository.hpp"
#include "FakePasswordHasher.hpp"

// TEST 1
TEST(AuthServiceTest, UserExistsReturnsTrueForExistingEmail)
{
	FakeUserRepository repository;
	FakePasswordHasher pwdHasher;

	repository.createUser(
		"test@test.com",
		"fakePwd123"
	);
	auth::AuthService service(repository, pwdHasher);
	EXPECT_TRUE(service.userExists("test@test.com"));
}

// TEST 2
TEST(AuthServiceTest, UserExistsReturnsFalseForUnknownEmail)
{
	FakeUserRepository repository;
	FakePasswordHasher pwdHasher;
	auth::AuthService service(repository, pwdHasher);
	EXPECT_FALSE(service.userExists("test@test.com"));
}

// TEST 3
TEST(AuthServiceTest, RegisterUserRejectsExistingEmail)
{
	FakeUserRepository repository;
	FakePasswordHasher hasher;

	repository.createUser(
		"test@test.com",
		"existing_hash"
	);
	auth::AuthService service(repository, hasher);
	const auto result = service.registerUser(
		"test@test.com",
		"Abcdefg1!"
	);
	EXPECT_EQ(
		result,
		auth::RegisterResult::EmailAlreadyExists
	);
}

// TEST 4
TEST(AuthServiceTest, RegisterUserRejectsInvalidEmail)
{
	FakeUserRepository repository;
	FakePasswordHasher hasher;
	auth::AuthService service(repository, hasher);
	const auto result = service.registerUser(
		"aaaa",
		"pwd123"
	);
	EXPECT_EQ(
		result,
		auth::RegisterResult::InvalidEmail
	);
}

// TEST 5
TEST(AuthServiceTest, RegisterUserSucceeds)
{
	FakeUserRepository repository;
	FakePasswordHasher hasher;
	auth::AuthService service(repository, hasher);
	const auto result = service.registerUser(
		"new@example.com",
		"Abcdefg1!"
	);
	EXPECT_EQ(
		result,
		auth::RegisterResult::Success
	);
	EXPECT_TRUE(
		service.userExists("new@example.com")
	);
}

// TEST 6
TEST(AuthServiceTest, RegisterUserHashesPasswordBeforeCreatingUser)
{
	FakeUserRepository repository;
	FakePasswordHasher hasher;
	auth::AuthService service(repository, hasher);
	service.registerUser(
		"new@example.com",
		"Abcdefg1!"
	);
	ASSERT_EQ(repository.users().size(), 1);
	EXPECT_EQ(
		repository.users()[0].passwordHash,
		"hashed_Abcdefg1!"
	);
}

// TEST 7
TEST(AuthServiceTest, RegisterUserReturnsCreationFailedWhenRepositoryFails)
{
	FakeUserRepository repository;
	FakePasswordHasher hasher;
	repository.failCreateUser = true;
	auth::AuthService service(repository, hasher);
	const auto result = service.registerUser(
		"test@example.com",
		"Abcdefg1!"
	);
	EXPECT_EQ(
		result,
		auth::RegisterResult::InternalError
	);
}

// TEST 8
TEST(AuthServiceTest, RegisterUserPassesPasswordToHasher)
{
	FakeUserRepository repository;
	FakePasswordHasher hasher;
	auth::AuthService service(repository, hasher);
	service.registerUser(
		"test@example.com",
		"Abcdefg1!"
	);
	EXPECT_EQ(
		hasher.lastPwd,
		"Abcdefg1!"
	);
}

// TEST 9
TEST(AuthServiceTest, LoginRejectsUnknownEmail)
{
	FakeUserRepository repository;
	FakePasswordHasher pwdHasher;
	auth::AuthService authService(
		repository,
		pwdHasher
	);
	const auto result = authService.login(
		"unknown@example.com",
		"my_password"
	);
	EXPECT_EQ(
		result.status,
		auth::LoginResult::Status::InvalidCredentials
	);
	EXPECT_FALSE(result.user.has_value());
}

// TEST 10
TEST(AuthServiceTest, LoginRejectsWrongPassword)
{
	FakeUserRepository repository;
	FakePasswordHasher pwdHasher;
	repository.createUser(
		"user@example.com",
		"hashed_correct_password"
	);
	auth::AuthService authService(
		repository,
		pwdHasher
	);
	const auto result = authService.login(
		"user@example.com",
		"wrong_password"
	);
	EXPECT_EQ(
		result.status,
		auth::LoginResult::Status::InvalidCredentials
	);
	EXPECT_FALSE(result.user.has_value());
}

// TEST 11
TEST(AuthServiceTests, LoginSucceedsWithCorrectCredentials)
{
	FakeUserRepository repository;
	FakePasswordHasher pwdHasher;
	const auto userResult = repository.createUser(
		"user@example.com",
		"hashed_pwd"
	);
	EXPECT_EQ(userResult.status, auth::CreateUserResult::Status::Success);
	ASSERT_TRUE(userResult.userId.has_value());
	auth::AuthService authService(
		repository,
		pwdHasher
	);
	const auto result = authService.login(
		"user@example.com",
		"pwd"
	);
	ASSERT_EQ(
		result.status,
		auth::LoginResult::Status::Success
	);
	ASSERT_TRUE(result.user.has_value());
	EXPECT_EQ(
		result.user->id,
		*(userResult.userId)
	);
	EXPECT_EQ(
		result.user->email,
		"user@example.com"
	);
}

// TEST 12
TEST(AuthServiceTest, LoginReturnsInternalErrorWhenRepositoryFails)
{
	FakeUserRepository repository;
	FakePasswordHasher pwdHasher;
	repository.failFindUser = true;
	auth::AuthService authService(
		repository,
		pwdHasher
	);
	const auto result = authService.login(
		"user@example.com",
		"my_password"
	);
	EXPECT_EQ(
		result.status,
		auth::LoginResult::Status::InternalError
	);
	EXPECT_FALSE(result.user.has_value());
}

// TEST 13
TEST(AuthServiceTest, RegisterUserReturnsEmailAlreadyExistsWhenCreationReportsDuplicate)
{
	FakeUserRepository repository;
	FakePasswordHasher pwdHasher;
	auth::AuthService service(repository, pwdHasher);
	repository.forcedStatus =
		auth::CreateUserResult::Status::EmailAlreadyExists;
	const auto result = service.registerUser(
		"test@example.com",
		"Abcdefg1!"
	);
	EXPECT_EQ(
		result,
		auth::RegisterResult::EmailAlreadyExists
	);
}

// TEST 14
TEST(AuthServiceTest, RegisterUserReturnsInternalErrorWhenHashingFails)
{
	FakeUserRepository repository;
	FakePasswordHasher pwdHasher;
	auth::AuthService authService(repository, pwdHasher);
	pwdHasher.failHash = true;
	const auto result = authService.registerUser(
		"test@example.com",
		"Abcdefg1!"
	);
	EXPECT_EQ(
		result,
		auth::RegisterResult::InternalError
	);
}

// TEST 15
TEST(AuthServiceTest, RegisterUserRejectsShortPassword)
{
    FakeUserRepository repository;
    FakePasswordHasher hasher;
    auth::AuthService service(repository, hasher);

    const auto result = service.registerUser(
        "test@example.com",
        "Ab1!xyz"
    );

    EXPECT_EQ(
        result,
        auth::RegisterResult::InvalidPassword
    );
}

// TEST 16
TEST(AuthServiceTest, RegisterUserRejectsPasswordWithoutUppercase)
{
    FakeUserRepository repository;
    FakePasswordHasher hasher;
    auth::AuthService service(repository, hasher);

    const auto result = service.registerUser(
        "test@example.com",
        "abcdefg1!"
    );

    EXPECT_EQ(
        result,
        auth::RegisterResult::InvalidPassword
    );
}

// TEST 17
TEST(AuthServiceTest, RegisterUserRejectsPasswordWithoutLowercase)
{
    FakeUserRepository repository;
    FakePasswordHasher hasher;
    auth::AuthService service(repository, hasher);

    const auto result = service.registerUser(
        "test@example.com",
        "ABCDEFG1!"
    );

    EXPECT_EQ(
        result,
        auth::RegisterResult::InvalidPassword
    );
}

// TEST 18
TEST(AuthServiceTest, RegisterUserRejectsPasswordWithoutDigit)
{
    FakeUserRepository repository;
    FakePasswordHasher hasher;
    auth::AuthService service(repository, hasher);

    const auto result = service.registerUser(
        "test@example.com",
        "Abcdefgh!"
    );

    EXPECT_EQ(
        result,
        auth::RegisterResult::InvalidPassword
    );
}

// TEST 19
TEST(AuthServiceTest, RegisterUserRejectsPasswordWithoutSpecialCharacter)
{
    FakeUserRepository repository;
    FakePasswordHasher hasher;
    auth::AuthService service(repository, hasher);

    const auto result = service.registerUser(
        "test@example.com",
        "Abcdefgh1"
    );

    EXPECT_EQ(
        result,
        auth::RegisterResult::InvalidPassword
    );
}

// TEST 20
TEST(AuthServiceTest, RegisterUserAcceptsPasswordWithExactlyEightCharacters)
{
    FakeUserRepository repository;
    FakePasswordHasher hasher;
    auth::AuthService service(repository, hasher);

    const auto result = service.registerUser(
        "test@example.com",
        "Abcdef1!"
    );
    EXPECT_EQ(
        result,
        auth::RegisterResult::Success
    );
}