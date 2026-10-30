#pragma once
#include <core/Logger.hpp>
#include <core/Result.hpp>
#include <pg/Pool.hpp>
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace pg
{
	// Un fichier de migration : 007_create_likes.sql → version 7.
	struct MigrationFile
	{
		int version;
		std::string name; // le nom du fichier, pour les messages
		std::filesystem::path path;
	};

	// Les fichiers NNN_nom.sql d'un dossier, triés par numéro (pas par nom :
	// 10_x.sql vient après 9_y.sql). Invalid si un nom ne suit pas ce format,
	// ou si deux fichiers portent le même numéro.
	core::Result<std::vector<MigrationFile>> listMigrations(const std::filesystem::path &directory);

	struct MigrationOptions
	{
		std::filesystem::path directory = "migrations";
		std::string table = "schema_migrations"; // [a-z_][a-z0-9_]*
		bool dryRun = false;					 // tout appliquer, puis ROLLBACK
	};

	struct MigrationReport
	{
		std::vector<std::string> applied; // appliquées par cet appel (ou qui l'auraient été)
		std::size_t upToDate = 0;		  // déjà appliquées, somme de contrôle vérifiée
	};

	// Applique les migrations manquantes, dans l'ordre, en UNE transaction :
	// toutes ou aucune. Un verrou consultatif empêche deux serveurs de migrer en
	// même temps. Une migration déjà appliquée dont le fichier a changé depuis
	// est refusée (Invalid) : on ne modifie pas l'histoire, on ajoute un fichier.
	core::Result<MigrationReport> migrate(ConnectionPool &pool, const MigrationOptions &options,
										  core::ILogger &logger);
}