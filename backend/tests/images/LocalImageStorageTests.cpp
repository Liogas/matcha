#include <gtest/gtest.h>

#include "../../src/images/LocalImageStorage.hpp"

#include <filesystem>

namespace
{
	class LocalImageStorageTest : public ::testing::Test
	{
		protected:
			void SetUp() override
			{
				root = std::filesystem::temp_directory_path() /
					"matcha-local-storage-tests";
				std::filesystem::remove_all(root);
			}

			void TearDown() override
			{
				std::filesystem::remove_all(root);
			}

			std::filesystem::path root;
	};
}

TEST_F(LocalImageStorageTest, RejectsPathEscape)
{
	LocalImageStorage storage(root);
	EXPECT_THROW(storage.put("../escape.webp", {1}), std::invalid_argument);
	EXPECT_THROW(storage.get("other/../../escape.webp"), std::invalid_argument);
}

TEST_F(LocalImageStorageTest, RemovesOnlyUnreferencedFiles)
{
	LocalImageStorage storage(root);
	storage.put("accounts/1/kept.webp", {1, 2, 3});
	storage.put("accounts/1/orphan.webp", {4, 5, 6});

	storage.removeUnreferenced({"accounts/1/kept.webp"});

	EXPECT_EQ(storage.get("accounts/1/kept.webp"),
		(std::vector<std::uint8_t>{1, 2, 3}));
	EXPECT_THROW(storage.get("accounts/1/orphan.webp"), std::runtime_error);
}
