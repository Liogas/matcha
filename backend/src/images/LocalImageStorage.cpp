#include "LocalImageStorage.hpp"

#include <fstream>
#include <random>
#include <stdexcept>

namespace
{
	std::string randomSuffix()
	{
		std::random_device device;
		return std::to_string(
			static_cast<unsigned long long>(device()) << 32 | device()
		);
	}
}

LocalImageStorage::LocalImageStorage(std::filesystem::path root):
	_root(std::move(root))
{
	std::filesystem::create_directories(_root);
}

std::filesystem::path LocalImageStorage::safePath(
	const std::string &key
) const
{
	if (key.empty() || key.front() == '/' ||
		key.find("..") != std::string::npos)
		throw std::invalid_argument("Invalid image storage key");
	const auto path = _root / key;
	const auto normalizedRoot = std::filesystem::weakly_canonical(_root);
	const auto normalizedPath = std::filesystem::weakly_canonical(path);
	const auto relative = normalizedPath.lexically_relative(normalizedRoot);
	if (relative.empty() || relative == ".." ||
		relative.begin()->string() == "..")
		throw std::invalid_argument("Image storage key escapes storage root");
	return path;
}

void LocalImageStorage::put(
	const std::string &key,
	const std::vector<std::uint8_t> &content
)
{
	const auto path = safePath(key);
	std::filesystem::create_directories(path.parent_path());
	const auto temporary = path.string() + ".tmp-" + randomSuffix();
	{
		std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
		if (!file)
			throw std::runtime_error("Unable to create image file");
		file.write(
			reinterpret_cast<const char *>(content.data()),
			static_cast<std::streamsize>(content.size())
		);
		if (!file)
			throw std::runtime_error("Unable to write image file");
	}
	std::filesystem::rename(temporary, path);
}

std::vector<std::uint8_t> LocalImageStorage::get(
	const std::string &key
) const
{
	const auto path = safePath(key);
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file)
		throw std::runtime_error("Image file not found");
	const auto size = file.tellg();
	if (size < 0)
		throw std::runtime_error("Unable to read image file size");
	std::vector<std::uint8_t> content(static_cast<std::size_t>(size));
	file.seekg(0);
	file.read(
		reinterpret_cast<char *>(content.data()),
		static_cast<std::streamsize>(content.size())
	);
	if (!file)
		throw std::runtime_error("Unable to read image file");
	return content;
}

void LocalImageStorage::remove(const std::string &key)
{
	std::error_code error;
	std::filesystem::remove(safePath(key), error);
	if (error)
		throw std::runtime_error("Unable to remove image file");
}

void LocalImageStorage::removeUnreferenced(
	const std::unordered_set<std::string> &referencedKeys)
{
	if (!std::filesystem::exists(_root))
		return;
	for (const auto &entry :
		std::filesystem::recursive_directory_iterator(_root))
	{
		if (!entry.is_regular_file())
			continue;
		const auto relative = entry.path().lexically_relative(_root);
		const auto key = relative.generic_string();
		if (referencedKeys.contains(key))
			continue;
		std::error_code error;
		std::filesystem::remove(entry.path(), error);
		if (error)
			throw std::runtime_error(
				"Unable to remove orphaned image file");
	}
}
