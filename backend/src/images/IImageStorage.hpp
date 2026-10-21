#pragma once

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

class IImageStorage
{
	public:
		virtual ~IImageStorage() = default;

		virtual void put(
			const std::string &key,
			const std::vector<std::uint8_t> &content
		) = 0;
		virtual std::vector<std::uint8_t> get(
			const std::string &key
		) const = 0;
		virtual void remove(const std::string &key) = 0;
		virtual void removeUnreferenced(
			const std::unordered_set<std::string> &referencedKeys) = 0;
};
