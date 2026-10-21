#pragma once

#include "../images/IImageStorage.hpp"
#include "../images/ImageProcessor.hpp"
#include "../repositories/ProfileRepository.hpp"

#include <cstdint>
#include <string>
#include <vector>

class ProfilePhotoService
{
	public:
		ProfilePhotoService(
			ProfileRepository &repository,
			ImageProcessor &processor,
			IImageStorage &storage
		);

		ProfilePhoto upload(
			std::int64_t accountId,
			std::int16_t position,
			const std::vector<std::uint8_t> &input,
			const std::string &contentType
		);
		bool replace(
			std::int64_t accountId,
			std::int64_t photoId,
			const std::vector<std::uint8_t> &input,
			const std::string &contentType
		);
		bool remove(std::int64_t accountId, std::int64_t photoId);
		bool setPrimary(std::int64_t accountId, std::int64_t photoId);
		std::vector<ProfilePhoto> list(std::int64_t accountId);
		std::vector<std::uint8_t> read(const std::string &storageKey);
		void cleanupOrphanedFiles();

	private:
		std::string newStorageKey(std::int64_t accountId) const;
		ProfileRepository &_repository;
		ImageProcessor &_processor;
		IImageStorage &_storage;
};
