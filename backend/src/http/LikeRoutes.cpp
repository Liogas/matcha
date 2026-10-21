#include "LikeRoutes.hpp"

#include "AuthRouteSupport.hpp"

#include <iostream>
#include <stdexcept>

namespace
{
crow::response unauthorized()
{
	return auth_routes::jsonError(401, "Missing or invalid bearer token");
}

bool validProfileTarget(ProfileRepository &repository,
	std::int64_t currentAccountId, std::int64_t targetAccountId)
{
	const auto profile = repository.findProfile(targetAccountId);
	if (!profile || !profile->completed || targetAccountId == currentAccountId)
		return false;
	return true;
}

}

void registerLikeRoutes(AuthApp &app, auth::Authenticator &authenticator,
	LikeRepository &likeRepository, ProfileRepository &profileRepository,
	MessagingRepository &messagingRepository)
{
	CROW_ROUTE(app, "/api/profiles/<int>/like")
		.methods(crow::HTTPMethod::POST)(
		[&](const crow::request &request, int targetId) {
			const auto current = auth_routes::authenticatedAccountId(request, authenticator);
			if (!current)
				return unauthorized();
			if (!validProfileTarget(profileRepository, *current, targetId))
				return auth_routes::jsonError(404, "Profile not found");
			try
			{
				const auto result = likeRepository.like(*current, targetId);
				if (result.matched &&
					!messagingRepository.ensureDirectConversation(
						*current, targetId))
					return auth_routes::jsonError(
						500, "Unable to create match conversation");
				crow::json::wvalue body;
				body["liked"] = result.liked;
				body["matched"] = result.matched;
				if (result.matchId)
					body["match_id"] = *result.matchId;
				return crow::response(200, body);
			}
			catch (const std::exception &error)
			{
				std::cerr << "[LIKE ERROR] " << error.what() << std::endl;
				return auth_routes::jsonError(500, "Unable to process like");
			}
		});

	CROW_ROUTE(app, "/api/profiles/<int>/like")
		.methods(crow::HTTPMethod::DELETE)(
		[&](const crow::request &request, int targetId) {
			const auto current = auth_routes::authenticatedAccountId(request, authenticator);
			if (!current)
				return unauthorized();
			try
			{
				likeRepository.unlike(*current, targetId);
				messagingRepository.removeDirectConversation(*current, targetId);
				return crow::response(204);
			}
			catch (const std::exception &error)
			{
				std::cerr << "[LIKE ERROR] " << error.what() << std::endl;
				return auth_routes::jsonError(500, "Unable to remove like");
			}
		});

	CROW_ROUTE(app, "/api/likes")
		.methods(crow::HTTPMethod::GET)(
		[&](const crow::request &request) {
			const auto current = auth_routes::authenticatedAccountId(request, authenticator);
			if (!current)
				return unauthorized();
			crow::json::wvalue::list liked;
			for (const auto id : likeRepository.listLikedAccountIds(*current))
				liked.emplace_back(id);
			crow::json::wvalue body;
			body["liked_account_ids"] = std::move(liked);
			return crow::response(200, body);
		});

	CROW_ROUTE(app, "/api/likes/received")
		.methods(crow::HTTPMethod::GET)(
		[&](const crow::request &request) {
			const auto current = auth_routes::authenticatedAccountId(request, authenticator);
			if (!current)
				return unauthorized();
			crow::json::wvalue::list liked;
			for (const auto id :
				likeRepository.listIncomingLikeAccountIds(*current))
				liked.emplace_back(id);
			crow::json::wvalue body;
			body["liker_account_ids"] = std::move(liked);
			return crow::response(200, body);
		});

	CROW_ROUTE(app, "/api/matches")
		.methods(crow::HTTPMethod::GET)(
		[&](const crow::request &request) {
			const auto current = auth_routes::authenticatedAccountId(request, authenticator);
			if (!current)
				return unauthorized();
			crow::json::wvalue::list matches;
			for (const auto &match : likeRepository.listMatches(*current))
			{
				crow::json::wvalue item;
				item["id"] = match.id;
				item["other_account_id"] = match.otherAccountId;
				matches.emplace_back(std::move(item));
			}
			crow::json::wvalue body;
			body["matches"] = std::move(matches);
			return crow::response(200, body);
		});
}
