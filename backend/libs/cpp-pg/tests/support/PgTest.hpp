#pragma once
#include <core/Logger.hpp>
#include <pg/Pool.hpp>
#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <sstream>

// Base des tests qui parlent à un vrai PostgreSQL (variables PG_TEST_*).
// Sans PG_TEST_HOST, ces tests sont sautés : la lib reste testable sans base.
// Chaque test commence avec une table `items` vide.
class PgTest : public testing::Test
{
protected:
	void SetUp() override;
	std::unique_ptr<pg::ConnectionPool> openPool(std::size_t size);

	// Membres détruits dans l'ordre inverse : le pool avant le logger qu'il utilise.
	pg::ConnectionSettings settings;
	std::optional<pg::Connection> connection;
	std::ostringstream logs;
	core::StreamLogger logger{logs, core::LogLevel::Debug};
	std::unique_ptr<pg::ConnectionPool> pool;
};