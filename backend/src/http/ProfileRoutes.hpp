#pragma once

#include "AuthRoutes.hpp"
#include "../images/ImageProcessor.hpp"
#include "../images/IImageStorage.hpp"
#include "../repositories/ProfileRepository.hpp"
#include "../services/ProfilePhotoService.hpp"

void registerProfileRoutes(
	AuthApp &app,
	auth::Authenticator &authenticator,
	ProfileRepository &profileRepository,
	ProfilePhotoService &photoService
);
