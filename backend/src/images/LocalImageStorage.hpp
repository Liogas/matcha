#pragma once

#include "IImageStorage.hpp"

#include <filesystem>

class LocalImageStorage : public IImageStorage
{
	public:
		explicit LocalImageStorage(std::filesystem::path root);

		void put(
			const std::string &key,
			const std::vector<std::uint8_t> &content
		) override;
		std::vector<std::uint8_t> get(
			const std::string &key
		) const override;
		void remove(const std::string &key) override;
		void removeUnreferenced(
			const std::unordered_set<std::string> &referencedKeys
		) override;

	private:
		std::filesystem::path safePath(const std::string &key) const;
		std::filesystem::path _root;
};
