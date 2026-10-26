#include <auth/session/InMemorySessionStore.hpp>
#include <auth/session/SessionManager.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

namespace
{
    TEST(SessionManagerTests, CreatesAndFindsSession)
    {
        auth::InMemorySessionStore store;
        auth::SessionManager manager(store);

        const auto now = std::chrono::system_clock::now();
        const auto expiry = now + std::chrono::minutes{15};

        ASSERT_TRUE(manager.createSession("token-123", "alice", expiry));

        const auto found = manager.findSession("token-123");
        ASSERT_TRUE(found.has_value());
        EXPECT_EQ(found->subject, "alice");
        EXPECT_EQ(found->expiresAt, expiry);
    }

    TEST(SessionManagerTests, RejectsDuplicateSession)
    {
        auth::InMemorySessionStore store;
        auth::SessionManager manager(store);

        const auto now = std::chrono::system_clock::now();

        ASSERT_TRUE(manager.createSession("token-123", "alice", now + std::chrono::minutes{5}));
        EXPECT_FALSE(manager.createSession("token-123", "bob", now + std::chrono::minutes{10}));
    }

    TEST(SessionManagerTests, RejectsEmptyTokenOrSubject)
    {
        auth::InMemorySessionStore store;
        auth::SessionManager manager(store);
        const auto expiry = std::chrono::system_clock::now() +
            std::chrono::minutes{5};

        EXPECT_FALSE(manager.createSession("", "alice", expiry));
        EXPECT_FALSE(manager.createSession("token-123", "", expiry));
        EXPECT_FALSE(manager.findSession("").has_value());
        EXPECT_FALSE(manager.revokeSession(""));
    }

    TEST(SessionManagerTests, InMemoryStoreRejectsAlreadyExpiredRecords)
    {
        auth::InMemorySessionStore store;
        const auto expiredRecord = auth::SessionRecord{
            "token-hash",
            "alice",
            std::chrono::system_clock::now() - std::chrono::minutes{1}
        };

        EXPECT_FALSE(store.create(expiredRecord));
        EXPECT_FALSE(store.findByTokenHash("token-hash").has_value());
    }

    TEST(SessionManagerTests, InMemoryStoreSupportsConcurrentCreation)
    {
        auth::InMemorySessionStore store;
        auth::SessionManager manager(store);
        std::atomic<bool> createdAll{true};
        std::vector<std::thread> workers;
        const auto now = std::chrono::system_clock::now();
        const auto expiresAt = now + std::chrono::minutes{5};

        constexpr int threadCount = 8;
        constexpr int sessionsPerThread = 32;
        for (int threadIndex = 0; threadIndex < threadCount; ++threadIndex)
        {
            workers.emplace_back([&, threadIndex]()
            {
                for (int sessionIndex = 0;
                    sessionIndex < sessionsPerThread;
                    ++sessionIndex)
                {
                    const std::string token =
                        "token-" + std::to_string(threadIndex) + "-" +
                        std::to_string(sessionIndex);
                    if (!manager.createSession(
                            token,
                            "user",
                            expiresAt
                        ))
                        createdAll = false;
                }
            });
        }

        for (auto &worker : workers)
            worker.join();

        ASSERT_TRUE(createdAll);
        for (int threadIndex = 0; threadIndex < threadCount; ++threadIndex)
        {
            for (int sessionIndex = 0;
                sessionIndex < sessionsPerThread;
                ++sessionIndex)
            {
                const std::string token =
                    "token-" + std::to_string(threadIndex) + "-" +
                    std::to_string(sessionIndex);
                EXPECT_TRUE(manager.isValid(token, now));
            }
        }
    }

    TEST(SessionManagerTests, RevokesAndValidatesSessionState)
    {
        auth::InMemorySessionStore store;
        auth::SessionManager manager(store);

        const auto now = std::chrono::system_clock::now();
        const auto future = now + std::chrono::minutes{5};
        const auto past = now - std::chrono::minutes{1};

        ASSERT_TRUE(manager.createSession("token-123", "alice", future));
        EXPECT_TRUE(manager.isValid("token-123", now));
        EXPECT_FALSE(manager.isExpired("token-123", now));

        ASSERT_TRUE(manager.revokeSession("token-123"));
        EXPECT_FALSE(manager.isValid("token-123", now));

        EXPECT_FALSE(manager.createSession("token-456", "bob", past));
        EXPECT_FALSE(manager.isValid("token-456", now));
        EXPECT_TRUE(manager.isExpired("token-456", now));
    }
}
