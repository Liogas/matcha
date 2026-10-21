#include "ProfileRoutes.hpp"

#include "../http/AuthRouteSupport.hpp"

#include <cctype>
#include <algorithm>
#include <charconv>
#include <cstring>
#include <chrono>
#include <iostream>
#include <cstdio>
#include <optional>
#include <regex>
#include <stdexcept>
#include <limits>

namespace
{
	crow::response unauthorized()
	{
		return auth_routes::jsonError(401, "Missing or invalid bearer token");
	}

	void addProfileBody(
		crow::json::wvalue &body,
		const ProfileData &profile,
		const std::vector<ProfilePhoto> &photos
	)
	{
		body["profile"]["account_id"] = profile.accountId;
		body["profile"]["username"] = profile.username;
		body["profile"]["first_name"] = profile.firstName;
		body["profile"]["last_name"] = profile.lastName;
		body["profile"]["date_of_birth"] =
			profile.dateOfBirth.value_or("");
		body["profile"]["gender"] = profile.gender.value_or("");
		body["profile"]["sexual_orientation"] =
			profile.sexualOrientation;
		body["profile"]["biography"] = profile.biography.value_or("");
		body["profile"]["city"] = profile.city.value_or("");
		body["profile"]["completed"] = profile.completed;
		crow::json::wvalue::list photoList;
		for (const auto &photo : photos)
		{
			crow::json::wvalue item;
			item["id"] = photo.id;
			item["position"] = photo.position;
			item["is_primary"] = photo.isPrimary;
			item["content_type"] = photo.metadata.contentType;
			item["file_size"] = static_cast<std::int64_t>(
				photo.metadata.fileSize
			);
			item["width"] = photo.metadata.width;
			item["height"] = photo.metadata.height;
			item["url"] = "/api/profile/photos/" +
				std::to_string(photo.id);
			photoList.emplace_back(std::move(item));
		}
		body["photos"] = std::move(photoList);
	}

	std::string stringField(
		const crow::json::rvalue &body,
		const char *name
	)
	{
		return body.has(name) &&
			body[name].t() == crow::json::type::String
			? std::string(body[name].s())
			: "";
	}

	bool hasStringField(
		const crow::json::rvalue &body,
		const char *name
	)
	{
		return body.has(name) &&
			body[name].t() == crow::json::type::String;
	}

	bool hasInvalidFieldType(
		const crow::json::rvalue &body,
		const char *name
	)
	{
		return body.has(name) && !hasStringField(body, name);
	}

	bool validDateOfBirth(const std::string &value)
	{
		int year = 0;
		unsigned month = 0;
		unsigned day = 0;
		char trailing = '\0';
		if (std::sscanf(value.c_str(), "%d-%u-%u%c",
				&year, &month, &day, &trailing) != 3)
			return false;
		const std::chrono::year_month_day date{
			std::chrono::year{year},
			std::chrono::month{month},
			std::chrono::day{day}};
		if (!date.ok())
			return false;
		const auto today = std::chrono::floor<std::chrono::days>(
			std::chrono::system_clock::now());
		return std::chrono::sys_days(date) <= today;
	}
}

void registerProfileRoutes(
	AuthApp &app,
	auth::Authenticator &authenticator,
	ProfileRepository &profileRepository,
	ProfilePhotoService &photoService
)
{
	CROW_ROUTE(app, "/api/profile")
		.methods(crow::HTTPMethod::GET)
	([&authenticator, &profileRepository](const crow::request &request)
	{
		const auto id = auth_routes::authenticatedAccountId(request, authenticator);
		if (!id)
			return unauthorized();
		const auto profile = profileRepository.findProfile(*id);
		if (!profile)
			return auth_routes::jsonError(404, "Profile not found");
		crow::json::wvalue body;
		addProfileBody(body, *profile, profileRepository.listPhotos(*id));
		return crow::response(200, body);
	});

	CROW_ROUTE(app, "/api/profiles")
		.methods(crow::HTTPMethod::GET)
	([&authenticator, &profileRepository](const crow::request &request)
	{
		const auto id = auth_routes::authenticatedAccountId(request, authenticator);
		if (!id)
			return unauthorized();
		const auto limitValue = request.url_params.get("limit");
		const auto offsetValue = request.url_params.get("offset");
		int limit = 24;
		int offset = 0;
		if (limitValue)
		{
			const auto parsed = std::from_chars(
				limitValue, limitValue + std::strlen(limitValue), limit);
			if (parsed.ec != std::errc{} || limit < 1 || limit > 50)
				return auth_routes::jsonError(400, "Limit must be between 1 and 50");
		}
		if (offsetValue)
		{
			const auto parsed = std::from_chars(
				offsetValue, offsetValue + std::strlen(offsetValue), offset);
			if (parsed.ec != std::errc{} || offset < 0)
				return auth_routes::jsonError(400, "Offset must be non-negative");
		}
		crow::json::wvalue::list profiles;
		for (const auto &profile :
			profileRepository.listCompletedProfiles(*id, limit, offset))
		{
			crow::json::wvalue item;
			item["account_id"] = profile.accountId;
			item["username"] = profile.username;
			item["first_name"] = profile.firstName;
			item["last_name"] = profile.lastName;
			item["date_of_birth"] = profile.dateOfBirth.value_or("");
			item["gender"] = profile.gender.value_or("");
			item["sexual_orientation"] = profile.sexualOrientation;
			item["biography"] = profile.biography.value_or("");
			item["city"] = profile.city.value_or("");
			item["completed"] = profile.completed;
			if (profile.primaryPhotoId)
				item["photo_url"] = "/api/profile/photos/" +
					std::to_string(*profile.primaryPhotoId);
			profiles.emplace_back(std::move(item));
		}
		crow::json::wvalue response;
		response["profiles"] = std::move(profiles);
		return crow::response(200, response);
	});

	CROW_ROUTE(app, "/api/profiles/<int>")
		.methods(crow::HTTPMethod::GET)
	([&authenticator, &profileRepository](
		const crow::request &request, int profileId)
	{
		const auto id = auth_routes::authenticatedAccountId(request, authenticator);
		if (!id)
			return unauthorized();
		const auto profile = profileRepository.findProfile(profileId);
		if (!profile || !profile->completed || profile->accountId == *id)
			return auth_routes::jsonError(404, "Profile not found");
		crow::json::wvalue body;
		addProfileBody(body, *profile, profileRepository.listPhotos(profileId));
		return crow::response(200, body);
	});

	CROW_ROUTE(app, "/api/profile")
		.methods(crow::HTTPMethod::PATCH)
	([&authenticator, &profileRepository](const crow::request &request)
	{
		const auto id = auth_routes::authenticatedAccountId(request, authenticator);
		if (!id)
			return unauthorized();
		const auto body = crow::json::load(request.body);
		if (!body || body.t() != crow::json::type::Object)
			return auth_routes::jsonError(400, "Invalid JSON");
		for (const auto *field : {
			"date_of_birth", "gender", "sexual_orientation",
			"biography", "city"
		})
		{
			if (hasInvalidFieldType(body, field))
				return auth_routes::jsonError(400, "Profile fields must be strings");
		}
		const auto current = profileRepository.findProfile(*id);
		if (!current)
			return auth_routes::jsonError(404, "Profile not found");

		const auto dateOfBirth = hasStringField(body, "date_of_birth")
			? stringField(body, "date_of_birth")
			: current->dateOfBirth.value_or("");
		const auto gender = hasStringField(body, "gender")
			? stringField(body, "gender")
			: current->gender.value_or("");
		const auto orientation = hasStringField(body, "sexual_orientation")
			? stringField(body, "sexual_orientation")
			: current->sexualOrientation;
		const auto biography = hasStringField(body, "biography")
			? stringField(body, "biography")
			: current->biography.value_or("");
		const auto city = hasStringField(body, "city")
			? auth_routes::trim(stringField(body, "city"))
			: current->city.value_or("");

		if (!dateOfBirth.empty() && !validDateOfBirth(dateOfBirth))
			return auth_routes::jsonError(400, "Invalid date of birth");
		if (orientation.empty())
			return auth_routes::jsonError(400, "Sexual orientation cannot be empty");
		if (!gender.empty() && !std::regex_match(
				gender, std::regex(R"(^[A-Za-z_]+$)")))
			return auth_routes::jsonError(400, "Invalid gender");
		if (!city.empty() && !std::regex_match(
				city, std::regex(
					R"(^[A-Za-zÀ-ÖØ-öø-ÿ][A-Za-zÀ-ÖØ-öø-ÿ .'-]*$)")))
			return auth_routes::jsonError(400, "Invalid city");
		try
		{
			if (!profileRepository.updateProfile(
					*id, dateOfBirth, gender, orientation, biography, city))
				return auth_routes::jsonError(500, "Unable to update profile");
			if (!profileRepository.refreshCompletion(*id))
				return auth_routes::jsonError(500, "Unable to refresh profile status");
		}
		catch (const std::exception &error)
		{
			std::cerr << "[PROFILE ERROR] Profile update failed: "
				<< error.what() << std::endl;
			return auth_routes::jsonError(400, "Invalid profile data");
		}
		const auto profile = profileRepository.findProfile(*id);
		if (!profile)
			return auth_routes::jsonError(500, "Unable to read profile");
		crow::json::wvalue response;
		addProfileBody(response, *profile, profileRepository.listPhotos(*id));
		return crow::response(200, response);
	});

	CROW_ROUTE(app, "/api/profile/photos")
		.methods(crow::HTTPMethod::POST)
	([&authenticator, &photoService, &profileRepository](
		const crow::request &request)
	{
		const auto id = auth_routes::authenticatedAccountId(request, authenticator);
		if (!id)
			return unauthorized();
		const auto positionValue = request.url_params.get("position");
		if (!positionValue)
			return auth_routes::jsonError(400, "Photo position is required");
		int position = 0;
		const auto parsed = std::from_chars(
			positionValue, positionValue + std::strlen(positionValue), position
		);
		if (parsed.ec != std::errc{} || position < 1 || position > 5)
			return auth_routes::jsonError(400, "Photo position must be 1-5");
		std::vector<std::uint8_t> content(
			request.body.begin(), request.body.end()
		);
		try
		{
			photoService.upload(
				*id, static_cast<std::int16_t>(position), content,
				request.get_header_value("Content-Type")
			);
			if (!profileRepository.refreshCompletion(*id))
				return auth_routes::jsonError(500, "Unable to refresh profile status");
			return auth_routes::jsonMessage(201, "Photo uploaded");
		}
		catch (const std::invalid_argument &error)
		{
			return auth_routes::jsonError(400, error.what());
		}
		catch (const std::exception &error)
		{
			std::cerr << "[PROFILE ERROR] Photo upload failed: "
				<< error.what() << std::endl;
			return auth_routes::jsonError(500, "Unable to store photo");
		}
	});

	CROW_ROUTE(app, "/api/profile/photos/<int>")
		.methods(crow::HTTPMethod::PUT)
	([&authenticator, &photoService](
		const crow::request &request, int photoId)
	{
		const auto id = auth_routes::authenticatedAccountId(request, authenticator);
		if (!id)
			return unauthorized();
		std::vector<std::uint8_t> content(
			request.body.begin(), request.body.end()
		);
		try
		{
			if (!photoService.replace(
					*id, photoId, content,
					request.get_header_value("Content-Type")))
				return auth_routes::jsonError(404, "Photo not found");
			return auth_routes::jsonMessage(200, "Photo replaced");
		}
		catch (const std::invalid_argument &error)
		{
			return auth_routes::jsonError(400, error.what());
		}
		catch (const std::exception &error)
		{
			std::cerr << "[PROFILE ERROR] Photo replacement failed: "
				<< error.what() << std::endl;
			return auth_routes::jsonError(500, "Unable to replace photo");
		}
	});

	CROW_ROUTE(app, "/api/profile/photos/<int>")
		.methods(crow::HTTPMethod::DELETE)
	([&authenticator, &photoService, &profileRepository](
		const crow::request &request, int photoId)
	{
		const auto id = auth_routes::authenticatedAccountId(request, authenticator);
		if (!id)
			return unauthorized();
		if (!photoService.remove(*id, photoId))
			return auth_routes::jsonError(404, "Photo not found");
		if (!profileRepository.refreshCompletion(*id))
			return auth_routes::jsonError(500, "Unable to refresh profile status");
		return auth_routes::jsonMessage(200, "Photo deleted");
	});

	CROW_ROUTE(app, "/api/profile/photos/<int>/primary")
		.methods(crow::HTTPMethod::POST)
	([&authenticator, &photoService, &profileRepository](
		const crow::request &request, int photoId)
	{
		const auto id = auth_routes::authenticatedAccountId(request, authenticator);
		if (!id)
			return unauthorized();
		if (!photoService.setPrimary(*id, photoId))
			return auth_routes::jsonError(404, "Photo not found");
		if (!profileRepository.refreshCompletion(*id))
			return auth_routes::jsonError(500, "Unable to refresh profile status");
		return auth_routes::jsonMessage(200, "Primary photo updated");
	});

	CROW_ROUTE(app, "/api/profile/photos/<int>")
		.methods(crow::HTTPMethod::GET)
	([&authenticator, &photoService, &profileRepository](
		const crow::request &request, int photoId)
	{
		const auto id = auth_routes::authenticatedAccountId(request, authenticator);
		if (!id)
			return unauthorized();
		const auto owner = profileRepository.findPhotoOwner(photoId);
		if (!owner)
			return auth_routes::jsonError(404, "Photo not found");
		const auto profile = profileRepository.findProfile(*owner);
		if (!profile || (!profile->completed && *owner != *id))
			return auth_routes::jsonError(404, "Photo not found");
		const auto photos = profileRepository.listPhotos(*owner);
		const auto photo = std::find_if(
			photos.begin(), photos.end(),
			[photoId](const ProfilePhoto &item) {
				return item.id == photoId;
			}
		);
		if (photo == photos.end())
			return auth_routes::jsonError(404, "Photo not found");
		try
		{
			const auto bytes = photoService.read(photo->storageKey);
			crow::response response(
				200,
				std::string(
					reinterpret_cast<const char *>(bytes.data()),
					bytes.size()
				)
			);
			response.set_header("Content-Type", photo->metadata.contentType);
			response.set_header("Cache-Control", "private, max-age=3600");
			return response;
		}
		catch (const std::exception &error)
		{
			std::cerr << "[PROFILE ERROR] Photo read failed: "
				<< error.what() << std::endl;
			return auth_routes::jsonError(404, "Photo not found");
		}
	});
}
