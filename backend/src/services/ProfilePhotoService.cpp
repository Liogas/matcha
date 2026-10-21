#include "ProfilePhotoService.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>

namespace
{
	void removeStoredFile(IImageStorage &storage, const std::string &key)
	{
		try
		{
			storage.remove(key);
		}
		catch (const std::exception &error)
		{
			std::cerr << "[PROFILE STORAGE WARNING] Unable to remove '"
				<< key << "': " << error.what() << std::endl;
		}
	}
}

ProfilePhotoService::ProfilePhotoService(
	ProfileRepository &repository,
	ImageProcessor &processor,
	IImageStorage &storage
):
	_repository(repository),
	_processor(processor),
	_storage(storage)
{}

std::string ProfilePhotoService::newStorageKey(std::int64_t accountId) const
{
	std::random_device device;
	std::ostringstream suffix;
	suffix << std::hex << std::setfill('0') << std::setw(16)
		<< (static_cast<std::uint64_t>(device()) << 32 | device());
	return "accounts/" + std::to_string(accountId) +
		"/photos/" + suffix.str() + ".webp";
}

ProfilePhoto ProfilePhotoService::upload(
	std::int64_t accountId,
	std::int16_t position,
	const std::vector<std::uint8_t> &input,
	const std::string &contentType
)
{
	if (position < 1 || position > 5)
		throw std::invalid_argument("Photo position must be between 1 and 5");
	const auto processed = _processor.process(input, contentType);
	const auto photos = _repository.listPhotos(accountId);
	const auto existing = std::find_if(
		photos.begin(), photos.end(),
		[position](const ProfilePhoto &photo) {
			return photo.position == position;
		}
	);
	const auto key = newStorageKey(accountId);
	_storage.put(key, processed.content);
	try
	{
		if (existing != photos.end())
		{
			const auto oldKey = existing->storageKey;
			if (!_repository.updatePhoto(
					accountId, existing->id, key, processed.metadata))
				throw std::runtime_error("Unable to update photo metadata");
			removeStoredFile(_storage, oldKey);
			return {
				existing->id, position, existing->isPrimary, key,
				processed.metadata
			};
		}
		const bool primary = photos.empty();
		const auto result = _repository.addPhoto(
			accountId, key, processed.metadata, position, primary);
		if (!result)
			throw std::runtime_error("Unable to save photo metadata");
		return *result;
	}
	catch (const std::exception &)
	{
		removeStoredFile(_storage, key);
		throw;
	}
}

bool ProfilePhotoService::replace(
	std::int64_t accountId,
	std::int64_t photoId,
	const std::vector<std::uint8_t> &input,
	const std::string &contentType
)
{
	const auto photos = _repository.listPhotos(accountId);
	const auto current = std::find_if(
		photos.begin(), photos.end(),
		[photoId](const ProfilePhoto &photo) { return photo.id == photoId; }
	);
	if (current == photos.end())
		return false;
	const auto processed = _processor.process(input, contentType);
	const auto key = newStorageKey(accountId);
	_storage.put(key, processed.content);
	try
	{
		if (!_repository.updatePhoto(
				accountId, photoId, key, processed.metadata))
			throw std::runtime_error("Unable to update photo metadata");
		removeStoredFile(_storage, current->storageKey);
		return true;
	}
	catch (const std::exception &)
	{
		removeStoredFile(_storage, key);
		throw;
	}
}

bool ProfilePhotoService::remove(std::int64_t accountId, std::int64_t photoId)
{
	const auto photos = _repository.listPhotos(accountId);
	const auto current = std::find_if(
		photos.begin(), photos.end(),
		[photoId](const ProfilePhoto &photo) { return photo.id == photoId; }
	);
	if (current == photos.end())
		return false;
	if (!_repository.deletePhoto(accountId, photoId))
		return false;
	removeStoredFile(_storage, current->storageKey);
	return true;
}

bool ProfilePhotoService::setPrimary(
	std::int64_t accountId,
	std::int64_t photoId
)
{
	return _repository.setPrimaryPhoto(accountId, photoId);
}

std::vector<ProfilePhoto> ProfilePhotoService::list(std::int64_t accountId)
{
	return _repository.listPhotos(accountId);
}

std::vector<std::uint8_t> ProfilePhotoService::read(const std::string &storageKey)
{
	return _storage.get(storageKey);
}

void ProfilePhotoService::cleanupOrphanedFiles()
{
	_storage.removeUnreferenced(_repository.listStorageKeys());
}
