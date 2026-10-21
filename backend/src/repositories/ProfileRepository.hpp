#pragma once

#include "../images/ImageTypes.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <unordered_set>

class Database;

struct ProfileData
{
	std::int64_t accountId;
	std::string username;
	std::string firstName;
	std::string lastName;
	std::optional<std::string> dateOfBirth;
	std::optional<std::string> gender;
	std::string sexualOrientation;
	std::optional<std::string> biography;
	std::optional<std::string> city;
	bool completed;
	std::optional<std::int64_t> primaryPhotoId;
};

struct ProfilePhoto
{
	std::int64_t id;
	std::int16_t position;
	bool isPrimary;
	std::string storageKey;
	ImageMetadata metadata;
};

class ProfileRepository
{
	public:
		explicit ProfileRepository(Database &database);

		std::optional<ProfileData> findProfile(std::int64_t accountId);
		std::vector<ProfileData> listCompletedProfiles(
			std::int64_t excludingAccountId,
			std::int32_t limit,
			std::int32_t offset
		);
		std::optional<std::int64_t> findPhotoOwner(std::int64_t photoId);
		bool updateProfile(
			std::int64_t accountId,
			const std::string &dateOfBirth,
			const std::string &gender,
			const std::string &sexualOrientation,
			const std::string &biography,
			const std::string &city
		);
		std::vector<ProfilePhoto> listPhotos(std::int64_t accountId);
		std::optional<ProfilePhoto> addPhoto(
			std::int64_t accountId,
			const std::string &storageKey,
			const ImageMetadata &metadata,
			std::int16_t position,
			bool isPrimary
		);
		bool updatePhoto(
			std::int64_t accountId,
			std::int64_t photoId,
			const std::string &storageKey,
			const ImageMetadata &metadata
		);
		bool deletePhoto(std::int64_t accountId, std::int64_t photoId);
		bool setPrimaryPhoto(std::int64_t accountId, std::int64_t photoId);
		bool hasPrimaryPhoto(std::int64_t accountId);
		bool refreshCompletion(std::int64_t accountId);
		std::unordered_set<std::string> listStorageKeys();

	private:
		Database &_database;
};
