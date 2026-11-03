#include "repositories/ProfileRepository.hpp"
#include "support/DatabaseTest.hpp"

#include <string>
#include <vector>

class ProfileRepositoryTest : public RepositoryTest
{
protected:
	// Un compte avec un profil complet (photo principale comprise).
	std::int64_t createCompletedProfile(const std::string &name)
	{
		const auto account = this->database().query(
			"INSERT INTO auth_accounts (email, password_hash) VALUES ($1, 'x') RETURNING id",
			name + "@example.test");
		EXPECT_TRUE(account) << account.error().message;
		const std::int64_t id = (*account)[0].get<std::int64_t>("id");
		EXPECT_TRUE(this->database().query(
			"INSERT INTO matcha_profiles (account_id, username, first_name, last_name, "
			"date_of_birth, gender, biography, city) "
			"VALUES ($1, $2, 'First', 'Last', '1995-01-01', 'woman', 'Hello', 'Paris')",
			id, name));
		EXPECT_TRUE(this->database().query(
			"INSERT INTO matcha_profile_photos (profile_account_id, storage_key, position, "
			"is_primary, file_size, width, height, file_hash) "
			"VALUES ($1, $2, 1, true, 10, 1, 1, repeat('a', 64))",
			id, name + ".webp"));
		return id;
	}

	std::vector<std::int64_t> page(ProfileRepository &repository, std::int64_t viewer,
								   std::optional<std::int64_t> after)
	{
		std::vector<std::int64_t> ids;
		for (const ProfileData &profile : repository.listCompletedProfiles(viewer, 2, after))
			ids.push_back(profile.accountId);
		return ids;
	}
};

TEST_F(ProfileRepositoryTest, PagesFollowTheCursor)
{
	ProfileRepository repository(this->database());
	const std::int64_t viewer = this->createCompletedProfile("viewer");
	const std::int64_t a = this->createCompletedProfile("profile_a");
	const std::int64_t b = this->createCompletedProfile("profile_b");
	const std::int64_t c = this->createCompletedProfile("profile_c");

	const auto first = this->page(repository, viewer, std::nullopt);
	ASSERT_EQ(first, (std::vector<std::int64_t>{a, b}));
	EXPECT_EQ(this->page(repository, viewer, first.back()), (std::vector<std::int64_t>{c}));
}

// Avec OFFSET, un profil qui disparaît de la première page décale tout :
// la deuxième page (OFFSET 2) sauterait c. Le curseur, lui, repart de b.
TEST_F(ProfileRepositoryTest, CursorDoesNotSkipWhenAProfileDisappears)
{
	ProfileRepository repository(this->database());
	const std::int64_t viewer = this->createCompletedProfile("viewer");
	const std::int64_t a = this->createCompletedProfile("profile_a");
	const std::int64_t b = this->createCompletedProfile("profile_b");
	const std::int64_t c = this->createCompletedProfile("profile_c");
	const std::int64_t d = this->createCompletedProfile("profile_d");

	const auto first = this->page(repository, viewer, std::nullopt);
	ASSERT_EQ(first, (std::vector<std::int64_t>{a, b}));
	// a supprime sa photo : son profil n'est plus complet.
	ASSERT_TRUE(this->database().query(
		"DELETE FROM matcha_profile_photos WHERE profile_account_id = $1", a));
	EXPECT_EQ(this->page(repository, viewer, first.back()), (std::vector<std::int64_t>{c, d}));
}