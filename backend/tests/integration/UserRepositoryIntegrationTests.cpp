#include <gtest/gtest.h>
#include "database/Database.hpp"
#include "repositories/UserRepository.hpp"

// TEST 1
TEST(UserRepositoryIntegrationTest, CanFindExistingUserByEmail)
{
	Database database;
	UserRepository repository(database);

	const std::string email = "integration_test@example.com";
	const std::string password = "fake_hash";

	database.executeParams(
		"DELETE FROM users WHERE email = $1",
		{email}
	);
	auto insertResult = database.executeParams(
		"INSERT INTO users (email, password_hash) "
		"VALUES ($1, $2)",
		{email, password}
	);
	ASSERT_FALSE(insertResult.isError());
	const auto result = repository.findByEmail(email);
	EXPECT_EQ(result.status, auth::UserLookupResult::Status::Found);
	ASSERT_TRUE(result.user.has_value());
	EXPECT_EQ(result.user->email, email);
	EXPECT_EQ(result.user->passwordHash, password);
}

// TEST 2
TEST(UserRepositoryIntegrationTest, CanCreateUser)
{
	Database database;
	UserRepository repository(database);
	const std::string email = "create_test@example.com";
	const std::string password = "fake_hash";
	database.executeParams(
		"DELETE FROM users WHERE email = $1",
		{email}
	);
	const auto userResult = repository.createUser(
		email,
		password
	);
	EXPECT_EQ(userResult.status, auth::CreateUserResult::Status::Success);
	ASSERT_TRUE(userResult.userId.has_value());
	EXPECT_GT(*(userResult.userId), 0);
	const auto result = repository.findByEmail(email);
	EXPECT_EQ(result.status, auth::UserLookupResult::Status::Found);
	ASSERT_TRUE(result.user.has_value());
	EXPECT_EQ(result.user->id, *(userResult.userId));
	EXPECT_EQ(result.user->email, email);
	EXPECT_EQ(result.user->passwordHash, password);
	database.executeParams(
		"DELETE FROM users"
		"WHERE email = $1",
		{email}
	);
}

// TEST 3
TEST(UserRepositoryIntegrationTest, CanFindExistingUserById)
{
	Database database;
	UserRepository repository(database);
	const std::string email = "find_by_id_test@example.com";
	const std::string password = "fake_hash";
	database.executeParams(
		"DELETE FROM users WHERE email = $1",
		{email}
	);
	const auto userResult = repository.createUser(
		email,
		password
	);
	EXPECT_EQ(userResult.status, auth::CreateUserResult::Status::Success);
	ASSERT_TRUE(userResult.userId.has_value());
	const auto result = repository.findById(*(userResult.userId));
	EXPECT_EQ(result.status, auth::UserLookupResult::Status::Found);
	ASSERT_TRUE(result.user.has_value());
	EXPECT_EQ(result.user->id, *(userResult.userId));
	EXPECT_EQ(result.user->email, email);
	EXPECT_EQ(result.user->passwordHash, password);
	database.executeParams(
		"DELETE FROM users WHERE email = $1",
		{email}
	);
}

// TEST 4
TEST(UserRepositoryIntegrationTest, CreateUserFailsWhenEmailAlreadyExists)
{
	Database database;
	UserRepository repository(database);
	const std::string email = "duplicate_test@example.com";
	const std::string pwd = "fake_hash";
	database.executeParams(
		"DELETE FROM users WHERE email = $1",
		{email}
	);
	const auto userResult = repository.createUser(
		email,
		pwd
	);
	EXPECT_EQ(userResult.status, auth::CreateUserResult::Status::Success);
	ASSERT_TRUE(userResult.userId.has_value());
	const auto userResult2 = repository.createUser(
		email,
		pwd
	);
	EXPECT_EQ(userResult2.status, auth::CreateUserResult::Status::EmailAlreadyExists);
	EXPECT_FALSE(userResult2.userId.has_value());
	database.executeParams(
		"DELETE FROM users WHERE email = $1",
		{email}
	);
}