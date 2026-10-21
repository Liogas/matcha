#pragma once

#include "AuthRoutes.hpp"
#include "../repositories/LikeRepository.hpp"
#include "../repositories/ProfileRepository.hpp"
#include "../messaging/MessagingRepository.hpp"
#include <auth/auth/Authenticator.hpp>

void registerLikeRoutes(
	AuthApp &app,
	auth::Authenticator &authenticator,
	LikeRepository &likeRepository,
	ProfileRepository &profileRepository,
	MessagingRepository &messagingRepository
);
