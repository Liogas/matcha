#include <gtest/gtest.h>
#include <auth/AuthService.hpp>
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
		"pwd123"
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
		"pwd123"
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
		"pwd123"
	);
	ASSERT_EQ(repository.users().size(), 1);
	EXPECT_EQ(
		repository.users()[0].passwordHash,
		"hashed_pwd123"
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
		"pwd123"
	);
	EXPECT_EQ(
		result,
		auth::RegisterResult::CreationFailed
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
		"pwd123"
	);
	EXPECT_EQ(
		hasher.lastPwd,
		"pwd123"
	);
}