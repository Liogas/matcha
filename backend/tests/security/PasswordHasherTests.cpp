#include <gtest/gtest.h>
#include "../../src/security/PasswordHasher.hpp"

// TEST 1
TEST(PasswordHasherTest, CanHashPassword)
{
	auth::PasswordHasher	passwordHasher;
	const std::string password = "my_password";
	const std::string hash = passwordHasher.hash(password);
	EXPECT_FALSE(hash.empty());
}

// TEST 2
TEST(PasswordHasherTest, SamePasswordProducesDifferentHashes)
{
	auth::PasswordHasher passwordHasher;
	const std::string password = "my_password";
	const std::string hash1 = passwordHasher.hash(password);
	const std::string hash2 = passwordHasher.hash(password);
	EXPECT_NE(hash1, hash2);
}

// TEST 3
TEST(PasswordHasherTest, VerifyCorrectPassword)
{
	auth::PasswordHasher passwordHasher;
	const std::string password = "my_password";
	const std::string hash = passwordHasher.hash(password);
	EXPECT_TRUE(
		passwordHasher.verify(password, hash)
	);
}

// TEST 4
TEST(PasswordHasherTest, RejectsWrongPassword)
{
	auth::PasswordHasher passwordHasher;
	const std::string password = "my_password";
	const std::string wrongPassword = "wrong_password";
	const std::string hash = passwordHasher.hash(password);
	EXPECT_FALSE(
		passwordHasher.verify(wrongPassword, hash)
	);
}

// TEST 5
TEST(PasswordHasherTest, RejectsInvalidHash)
{
	auth::PasswordHasher passwordHasher;
	EXPECT_FALSE(
		passwordHasher.verify(
			"my_password",
			"no_valid_hash"
		)
	);
}