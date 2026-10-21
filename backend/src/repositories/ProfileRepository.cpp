#include "ProfileRepository.hpp"

#include "../database/Database.hpp"

ProfileRepository::ProfileRepository(Database &database):
	_database(database)
{}

namespace
{
	std::optional<ProfileData> readProfile(
		const QueryResult &result,
		int row
	)
	{
		const auto id = result.getInt64(row, 0);
		const auto username = result.getString(row, 1);
		const auto firstName = result.getString(row, 2);
		const auto lastName = result.getString(row, 3);
		const auto orientation = result.getString(row, 6);
		const auto completed = result.getBool(row, 9);
		if (!id || !username || !firstName || !lastName ||
			!orientation || !completed)
			return std::nullopt;
		return ProfileData{
			*id, *username, *firstName, *lastName,
			result.getString(row, 4), result.getString(row, 5),
			*orientation, result.getString(row, 7),
			result.getString(row, 8), *completed,
			result.getInt64(row, 10)
		};
	}
}

std::optional<ProfileData> ProfileRepository::findProfile(
	std::int64_t accountId
)
{
	const auto result = _database.executeParams(
		"SELECT p.account_id, p.username, p.first_name, p.last_name, "
		"p.date_of_birth::text, p.gender, p.sexual_orientation, p.biography, "
		"p.city, (p.date_of_birth IS NOT NULL "
		"AND p.date_of_birth <= CURRENT_DATE - INTERVAL '18 years' "
		"AND p.gender IS NOT NULL "
		"AND NULLIF(BTRIM(p.biography), '') IS NOT NULL "
		"AND p.city IS NOT NULL "
		"AND p.city ~ '^[[:alpha:]][[:alpha:] .''-]*$' "
		"AND EXISTS (SELECT 1 FROM matcha_profile_photos photo "
		"WHERE photo.profile_account_id = p.account_id "
		"AND photo.is_primary)) "
		", (SELECT photo.id FROM matcha_profile_photos photo "
		"WHERE photo.profile_account_id = p.account_id "
		"AND photo.is_primary LIMIT 1) "
		"FROM matcha_profiles p WHERE p.account_id = $1",
		{std::to_string(accountId)}
	);
	if (result.isError() || result.rowCount() != 1)
		return std::nullopt;
	return readProfile(result, 0);
}

std::vector<ProfileData> ProfileRepository::listCompletedProfiles(
	std::int64_t excludingAccountId,
	std::int32_t limit,
	std::int32_t offset
)
{
	const auto result = _database.executeParams(
		"SELECT p.account_id, p.username, p.first_name, p.last_name, "
		"p.date_of_birth::text, p.gender, p.sexual_orientation, p.biography, "
		"p.city, TRUE, (SELECT photo.id FROM matcha_profile_photos photo "
		"WHERE photo.profile_account_id = p.account_id "
		"AND photo.is_primary LIMIT 1) "
		"FROM matcha_profiles p "
		"WHERE p.account_id <> $1 "
		"AND p.date_of_birth IS NOT NULL "
		"AND p.date_of_birth <= CURRENT_DATE - INTERVAL '18 years' "
		"AND p.gender IS NOT NULL "
		"AND NULLIF(BTRIM(p.biography), '') IS NOT NULL "
		"AND p.city IS NOT NULL "
		"AND p.city ~ '^[[:alpha:]][[:alpha:] .''-]*$' "
		"AND EXISTS (SELECT 1 FROM matcha_profile_photos photo "
		"WHERE photo.profile_account_id = p.account_id "
		"AND photo.is_primary) "
		"ORDER BY p.created_at, p.account_id LIMIT $2 OFFSET $3",
		{
			std::to_string(excludingAccountId),
			std::to_string(limit),
			std::to_string(offset)
		}
	);
	std::vector<ProfileData> profiles;
	if (result.isError())
		return profiles;
	for (int row = 0; row < result.rowCount(); ++row)
	{
		const auto profile = readProfile(result, row);
		if (profile)
			profiles.push_back(*profile);
	}
	return profiles;
}

std::optional<std::int64_t> ProfileRepository::findPhotoOwner(
	std::int64_t photoId
)
{
	const auto result = _database.executeParams(
		"SELECT profile_account_id FROM matcha_profile_photos "
		"WHERE id = $1",
		{std::to_string(photoId)}
	);
	if (result.isError() || result.rowCount() != 1)
		return std::nullopt;
	return result.getInt64(0, 0);
}

bool ProfileRepository::updateProfile(
	std::int64_t accountId,
	const std::string &dateOfBirth,
	const std::string &gender,
	const std::string &sexualOrientation,
	const std::string &biography,
	const std::string &city
)
{
	const auto result = _database.executeParams(
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
		{
			std::to_string(accountId),
			dateOfBirth,
			gender,
			sexualOrientation,
			biography,
			city
		}
	);
	return !result.isError() && result.rowCount() == 1;
}

std::vector<ProfilePhoto> ProfileRepository::listPhotos(
	std::int64_t accountId
)
{
	const auto result = _database.executeParams(
		"SELECT id, storage_key, position, is_primary, content_type, "
		"file_size, width, height, file_hash "
		"FROM matcha_profile_photos WHERE profile_account_id = $1 "
		"ORDER BY position",
		{std::to_string(accountId)}
	);
	std::vector<ProfilePhoto> photos;
	if (result.isError())
		return photos;
	for (int row = 0; row < result.rowCount(); ++row)
	{
		const auto id = result.getInt64(row, 0);
		const auto key = result.getString(row, 1);
		const auto position = result.getInt(row, 2);
		const auto primary = result.getBool(row, 3);
		const auto type = result.getString(row, 4);
		const auto size = result.getInt64(row, 5);
		const auto width = result.getInt(row, 6);
		const auto height = result.getInt(row, 7);
		const auto hash = result.getString(row, 8);
		if (!id || !key || !position || !primary || !type || !size ||
			!width || !height || !hash)
			continue;
		photos.push_back({
			*id,
			static_cast<std::int16_t>(*position),
			*primary,
			*key,
			{*type, static_cast<std::size_t>(*size),
				static_cast<std::uint32_t>(*width),
				static_cast<std::uint32_t>(*height), *hash}
		});
	}
	return photos;
}

std::optional<ProfilePhoto> ProfileRepository::addPhoto(
	std::int64_t accountId,
	const std::string &storageKey,
	const ImageMetadata &metadata,
	std::int16_t position,
	bool isPrimary
)
{
	const auto result = _database.executeParams(
		"INSERT INTO matcha_profile_photos "
		"(profile_account_id, storage_key, position, is_primary, content_type, "
		"file_size, width, height, file_hash) VALUES "
		"($1, $2, $3, $4, $5, $6, $7, $8, $9) "
		"RETURNING id",
		{
			std::to_string(accountId), storageKey, std::to_string(position),
			isPrimary ? "true" : "false", metadata.contentType,
			std::to_string(metadata.fileSize), std::to_string(metadata.width),
			std::to_string(metadata.height), metadata.sha256
		}
	);
	if (result.isError() || result.rowCount() != 1)
		return std::nullopt;
	const auto id = result.getInt64(0, 0);
	if (!id)
		return std::nullopt;
	return ProfilePhoto{*id, position, isPrimary, storageKey, metadata};
}

bool ProfileRepository::updatePhoto(
	std::int64_t accountId,
	std::int64_t photoId,
	const std::string &storageKey,
	const ImageMetadata &metadata
)
{
	const auto result = _database.executeParams(
		"UPDATE matcha_profile_photos SET storage_key = $3, "
		"content_type = $4, file_size = $5, width = $6, height = $7, "
		"file_hash = $8 WHERE id = $1 AND profile_account_id = $2 "
		"RETURNING id",
		{
			std::to_string(photoId), std::to_string(accountId), storageKey,
			metadata.contentType, std::to_string(metadata.fileSize),
			std::to_string(metadata.width), std::to_string(metadata.height),
			metadata.sha256
		}
	);
	return !result.isError() && result.rowCount() == 1;
}

bool ProfileRepository::deletePhoto(std::int64_t accountId, std::int64_t photoId)
{
	const auto result = _database.executeParams(
		"DELETE FROM matcha_profile_photos "
		"WHERE id = $1 AND profile_account_id = $2 RETURNING id",
		{std::to_string(photoId), std::to_string(accountId)}
	);
	return !result.isError() && result.rowCount() == 1;
}

bool ProfileRepository::setPrimaryPhoto(
	std::int64_t accountId,
	std::int64_t photoId
)
{
	const auto result = _database.executeParams(
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
		{std::to_string(accountId), std::to_string(photoId)}
	);
	return !result.isError() && result.rowCount() == 1;
}

bool ProfileRepository::hasPrimaryPhoto(std::int64_t accountId)
{
	const auto result = _database.executeParams(
		"SELECT 1 FROM matcha_profile_photos "
		"WHERE profile_account_id = $1 AND is_primary",
		{std::to_string(accountId)}
	);
	return !result.isError() && result.rowCount() == 1;
}

bool ProfileRepository::refreshCompletion(std::int64_t accountId)
{
	const auto result = _database.executeParams(
		"UPDATE matcha_profiles SET completed_at = CASE "
		"WHEN date_of_birth IS NOT NULL "
		"AND date_of_birth <= CURRENT_DATE - INTERVAL '18 years' "
		"AND gender IS NOT NULL AND NULLIF(BTRIM(biography), '') IS NOT NULL "
		"AND city IS NOT NULL "
		"AND city ~ '^[[:alpha:]][[:alpha:] .''-]*$' "
		"AND EXISTS (SELECT 1 FROM matcha_profile_photos "
		"WHERE profile_account_id = $1 AND is_primary) "
		"THEN COALESCE(completed_at, NOW()) ELSE NULL END, "
		"updated_at = NOW() WHERE account_id = $1 RETURNING account_id",
		{std::to_string(accountId)}
	);
	return !result.isError() && result.rowCount() == 1;
}

std::unordered_set<std::string> ProfileRepository::listStorageKeys()
{
	const auto result = _database.execute(
		"SELECT storage_key FROM matcha_profile_photos");
	std::unordered_set<std::string> keys;
	if (result.isError())
		return keys;
	for (int row = 0; row < result.rowCount(); ++row)
	{
		const auto key = result.getString(row, 0);
		if (key)
			keys.insert(*key);
	}
	return keys;
}
