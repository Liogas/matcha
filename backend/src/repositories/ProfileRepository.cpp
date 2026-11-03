#include "ProfileRepository.hpp"

#include <pg/QueryBuilder.hpp>

ProfileRepository::ProfileRepository(pg::Executor &database) : _database(database)
{
}

namespace
{
	// Les colonnes lues par readProfile, dans toutes les requêtes de profil.
	constexpr const char *PROFILE_COLUMNS =
		"p.account_id, p.username, p.first_name, p.last_name, "
		"p.date_of_birth::text AS date_of_birth, p.gender, p.sexual_orientation, "
		"p.biography, p.city, "
		"(SELECT photo.id FROM matcha_profile_photos photo "
		"WHERE photo.profile_account_id = p.account_id "
		"AND photo.is_primary LIMIT 1) AS primary_photo_id";

	// Un profil est complet quand ces conditions sont toutes vraies.
	constexpr const char *PROFILE_COMPLETED =
		"p.date_of_birth IS NOT NULL "
		"AND p.date_of_birth <= CURRENT_DATE - INTERVAL '18 years' "
		"AND p.gender IS NOT NULL "
		"AND NULLIF(BTRIM(p.biography), '') IS NOT NULL "
		"AND p.city IS NOT NULL "
		"AND p.city ~ '^[[:alpha:]][[:alpha:] .''-]*$' "
		"AND EXISTS (SELECT 1 FROM matcha_profile_photos photo "
		"WHERE photo.profile_account_id = p.account_id "
		"AND photo.is_primary)";

	// La recherche avancée ajoutera ici d'autres tris (âge, distance, popularité).
	const pg::SortWhitelist PROFILE_SORTS{{"account", "p.account_id"}};

	ProfileData readProfile(const pg::Row &row)
	{
		return ProfileData{
			row.get<std::int64_t>("account_id"),
			row.get<std::string>("username"),
			row.get<std::string>("first_name"),
			row.get<std::string>("last_name"),
			row.get<std::optional<std::string>>("date_of_birth"),
			row.get<std::optional<std::string>>("gender"),
			row.get<std::string>("sexual_orientation"),
			row.get<std::optional<std::string>>("biography"),
			row.get<std::optional<std::string>>("city"),
			row.get<bool>("completed"),
			row.get<std::optional<std::int64_t>>("primary_photo_id")};
	}
}

std::optional<ProfileData> ProfileRepository::findProfile(
	std::int64_t accountId)
{
	const auto result = _database.query(
		std::string("SELECT ") + PROFILE_COLUMNS + ", (" + PROFILE_COMPLETED +
			") AS completed FROM matcha_profiles p WHERE p.account_id = $1",
		accountId);
	if (!result || result->size() != 1)
		return std::nullopt;
	return readProfile((*result)[0]);
}

std::vector<ProfileData> ProfileRepository::listCompletedProfiles(
	std::int64_t excludingAccountId,
	std::int32_t limit,
	std::optional<std::int64_t> afterAccountId)
{
	pg::QueryBuilder builder(std::string("SELECT ") + PROFILE_COLUMNS +
							 ", TRUE AS completed FROM matcha_profiles p");
	builder.where("p.account_id <> ?", excludingAccountId)
		.where(PROFILE_COMPLETED)
		.whereIf(afterAccountId, "p.account_id > ?")
		.orderBy("account", PROFILE_SORTS)
		.limit(limit);
	const auto query = builder.build();
	std::vector<ProfileData> profiles;
	if (!query)
		return profiles;
	const auto result = _database.execute(query->sql, query->params);
	if (!result)
		return profiles;
	for (const pg::Row row : *result)
		profiles.push_back(readProfile(row));
	return profiles;
}

std::optional<std::int64_t> ProfileRepository::findPhotoOwner(
	std::int64_t photoId)
{
	const auto result = _database.query(
		"SELECT profile_account_id FROM matcha_profile_photos "
		"WHERE id = $1",
		photoId);
	if (!result || result->size() != 1)
		return std::nullopt;
	return (*result)[0].get<std::int64_t>("profile_account_id");
}

bool ProfileRepository::updateProfile(
	std::int64_t accountId,
	const std::string &dateOfBirth,
	const std::string &gender,
	const std::string &sexualOrientation,
	const std::string &biography,
	const std::string &city)
{
	const auto result = _database.query(
		"UPDATE matcha_profiles SET date_of_birth = NULLIF($2, '')::date, "
		"gender = NULLIF($3, ''), sexual_orientation = $4, "
		"biography = NULLIF($5, ''), city = NULLIF($6, ''), "
		"completed_at = CASE WHEN NULLIF($2, '') IS NOT NULL "
		"AND NULLIF($3, '') IS NOT NULL "
		"AND NULLIF(BTRIM($5), '') IS NOT NULL "
		"AND NULLIF($6, '') IS NOT NULL "
		"AND NULLIF(BTRIM($6), '') ~ '^[[:alpha:]][[:alpha:] .''-]*$' "
		"AND NULLIF($2, '')::date <= CURRENT_DATE - INTERVAL '18 years' "
		"AND EXISTS (SELECT 1 FROM matcha_profile_photos "
		"WHERE profile_account_id = $1 AND is_primary) "
		"THEN COALESCE(completed_at, NOW()) ELSE NULL END, "
		"updated_at = NOW() WHERE account_id = $1 "
		"RETURNING account_id",
		accountId,
		dateOfBirth,
		gender,
		sexualOrientation,
		biography,
		city);
	return result && result->size() == 1;
}

std::vector<ProfilePhoto> ProfileRepository::listPhotos(
	std::int64_t accountId)
{
	const auto result = _database.query(
		"SELECT id, storage_key, position, is_primary, content_type, "
		"file_size, width, height, file_hash "
		"FROM matcha_profile_photos WHERE profile_account_id = $1 "
		"ORDER BY position",
		accountId);
	std::vector<ProfilePhoto> photos;
	if (!result)
		return photos;
	for (const pg::Row row : *result)
	{
		photos.push_back({row.get<std::int64_t>("id"),
						  row.get<std::int16_t>("position"),
						  row.get<bool>("is_primary"),
						  row.get<std::string>("storage_key"),
						  {row.get<std::string>("content_type"),
						   row.get<std::size_t>("file_size"),
						   row.get<std::uint32_t>("width"),
						   row.get<std::uint32_t>("height"),
						   row.get<std::string>("file_hash")}});
	}
	return photos;
}

std::optional<ProfilePhoto> ProfileRepository::addPhoto(
	std::int64_t accountId,
	const std::string &storageKey,
	const ImageMetadata &metadata,
	std::int16_t position,
	bool isPrimary)
{
	const auto result = _database.query(
		"INSERT INTO matcha_profile_photos "
		"(profile_account_id, storage_key, position, is_primary, content_type, "
		"file_size, width, height, file_hash) VALUES "
		"($1, $2, $3, $4, $5, $6, $7, $8, $9) "
		"RETURNING id",
		accountId, storageKey, position, isPrimary, metadata.contentType,
		metadata.fileSize, metadata.width, metadata.height, metadata.sha256);
	if (!result || result->size() != 1)
		return std::nullopt;
	return ProfilePhoto{(*result)[0].get<std::int64_t>("id"), position,
						isPrimary, storageKey, metadata};
}

bool ProfileRepository::updatePhoto(
	std::int64_t accountId,
	std::int64_t photoId,
	const std::string &storageKey,
	const ImageMetadata &metadata)
{
	const auto result = _database.query(
		"UPDATE matcha_profile_photos SET storage_key = $3, "
		"content_type = $4, file_size = $5, width = $6, height = $7, "
		"file_hash = $8 WHERE id = $1 AND profile_account_id = $2 "
		"RETURNING id",
		photoId, accountId, storageKey, metadata.contentType,
		metadata.fileSize, metadata.width, metadata.height, metadata.sha256);
	return result && result->size() == 1;
}

bool ProfileRepository::deletePhoto(std::int64_t accountId, std::int64_t photoId)
{
	const auto result = _database.query(
		"DELETE FROM matcha_profile_photos "
		"WHERE id = $1 AND profile_account_id = $2 RETURNING id",
		photoId, accountId);
	return result && result->size() == 1;
}

bool ProfileRepository::setPrimaryPhoto(
	std::int64_t accountId,
	std::int64_t photoId)
{
	const auto result = _database.query(
		"WITH target AS ("
		"SELECT id FROM matcha_profile_photos "
		"WHERE id = $2 AND profile_account_id = $1"
		"), reset AS ("
		"UPDATE matcha_profile_photos SET is_primary = false "
		"WHERE profile_account_id = $1 "
		"AND EXISTS (SELECT 1 FROM target)"
		"), selected AS ("
		"UPDATE matcha_profile_photos SET is_primary = true "
		"WHERE id IN (SELECT id FROM target) RETURNING id"
		") SELECT id FROM selected",
		accountId, photoId);
	return result && result->size() == 1;
}

bool ProfileRepository::hasPrimaryPhoto(std::int64_t accountId)
{
	const auto result = _database.query(
		"SELECT 1 FROM matcha_profile_photos "
		"WHERE profile_account_id = $1 AND is_primary",
		accountId);
	return result && result->size() == 1;
}

bool ProfileRepository::refreshCompletion(std::int64_t accountId)
{
	const auto result = _database.query(
		std::string("UPDATE matcha_profiles p SET completed_at = CASE WHEN ") +
			PROFILE_COMPLETED +
			" THEN COALESCE(p.completed_at, NOW()) ELSE NULL END, "
			"updated_at = NOW() WHERE p.account_id = $1 RETURNING p.account_id",
		accountId);
	return result && result->size() == 1;
}

std::unordered_set<std::string> ProfileRepository::listStorageKeys()
{
	const auto result = _database.query(
		"SELECT storage_key FROM matcha_profile_photos");
	std::unordered_set<std::string> keys;
	if (!result)
		return keys;
	for (const pg::Row row : *result)
		keys.insert(row.get<std::string>("storage_key"));
	return keys;
}