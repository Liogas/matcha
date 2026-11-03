#include "services/MatchService.hpp"
#include "support/DatabaseTest.hpp"

#include <latch>
#include <string>
#include <thread>

// Ici, pas de transaction annulée : MatchService ouvre et valide ses propres
// transactions, sur deux threads à la fois. Les comptes créés sont supprimés
// à la fin de chaque test (ON DELETE CASCADE emporte likes, matchs et conversations).
class MatchServiceTest : public DatabaseTest
{
protected:
	std::size_t poolSize() const override { return 4; }

	void TearDown() override
	{
		if (this->pool)
			(void)this->pool->query("DELETE FROM auth_accounts WHERE email LIKE 'match-test-%'");
	}

	std::int64_t createAccount()
	{
		const auto rows = this->pool->query(
			"INSERT INTO auth_accounts (email, password_hash) VALUES ($1, 'x') RETURNING id",
			"match-test-" + std::to_string(++this->_created) + "@example.test");
		EXPECT_TRUE(rows) << rows.error().message;
		return rows ? (*rows)[0].get<std::int64_t>("id") : 0;
	}

	std::int64_t count(const char *sql, std::int64_t a, std::int64_t b)
	{
		const auto rows = this->pool->query(sql, a, b);
		EXPECT_TRUE(rows) << rows.error().message;
		return rows ? (*rows)[0].get<std::int64_t>("n") : -1;
	}

	std::int64_t matchesBetween(std::int64_t a, std::int64_t b)
	{
		return this->count("SELECT count(*) AS n FROM profile_matches "
						   "WHERE first_account_id = LEAST($1::BIGINT, $2::BIGINT) "
						   "AND second_account_id = GREATEST($1::BIGINT, $2::BIGINT)",
						   a, b);
	}

	std::int64_t conversationsBetween(std::int64_t a, std::int64_t b)
	{
		return this->count("SELECT count(*) AS n FROM messaging_conversation_participants p "
						   "JOIN messaging_conversation_participants q "
						   "ON q.conversation_id = p.conversation_id "
						   "WHERE p.account_id = $1 AND q.account_id = $2",
						   a, b);
	}

private:
	int _created = 0;
};

TEST_F(MatchServiceTest, MutualLikeCreatesMatchAndConversation)
{
	MatchService service(*this->pool);
	const std::int64_t alice = this->createAccount();
	const std::int64_t bob = this->createAccount();

	const auto first = service.like(alice, bob);
	ASSERT_TRUE(first) << first.error().message;
	EXPECT_FALSE(first->matched);
	const auto second = service.like(bob, alice);
	ASSERT_TRUE(second) << second.error().message;
	EXPECT_TRUE(second->matched);
	EXPECT_EQ(this->matchesBetween(alice, bob), 1);
	EXPECT_EQ(this->conversationsBetween(alice, bob), 1);
}

TEST_F(MatchServiceTest, UnlikeRemovesMatchAndConversation)
{
	MatchService service(*this->pool);
	const std::int64_t alice = this->createAccount();
	const std::int64_t bob = this->createAccount();
	ASSERT_TRUE(service.like(alice, bob));
	ASSERT_TRUE(service.like(bob, alice));

	const auto removed = service.unlike(alice, bob);
	ASSERT_TRUE(removed) << removed.error().message;
	EXPECT_EQ(this->matchesBetween(alice, bob), 0);
	EXPECT_EQ(this->conversationsBetween(alice, bob), 0);
}

// Le scénario de l'audit (4.6) : A like B et B like A au même instant. Sans
// le verrou de MatchService, chaque transaction ne voit pas le like (pas
// encore validé) de l'autre, et aucun match n'est créé.
TEST_F(MatchServiceTest, SimultaneousLikesCreateExactlyOneMatch)
{
	MatchService service(*this->pool);
	for (int round = 0; round < 20; ++round)
	{
		const std::int64_t alice = this->createAccount();
		const std::int64_t bob = this->createAccount();
		std::latch start(2);
		auto likeAtOnce = [&](std::int64_t liker, std::int64_t liked)
		{
			start.arrive_and_wait(); // les deux threads partent ensemble
			const auto result = service.like(liker, liked);
			EXPECT_TRUE(result) << result.error().message;
		};
		std::thread first(likeAtOnce, alice, bob);
		std::thread second(likeAtOnce, bob, alice);
		first.join();
		second.join();
		ASSERT_EQ(this->matchesBetween(alice, bob), 1) << "round " << round;
		ASSERT_EQ(this->conversationsBetween(alice, bob), 1) << "round " << round;
	}
}