#include <pg/Migrations.hpp>
#include <algorithm>
#include <charconv>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>

namespace
{
	// Le nom de la table est collé dans le SQL (un nom ne peut pas être un
	// paramètre $n) : on n'accepte que des caractères sans danger.
	bool isPlainIdentifier(std::string_view name)
	{
		if (name.empty() || name.size() > 63 || (name[0] >= '0' && name[0] <= '9'))
			return false;
		return std::ranges::all_of(name, [](char c)
								   { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; });
	}

	// "007_create_likes.sql" → 7 ; nullopt si le nom ne suit pas NNN_nom.sql.
	std::optional<int> versionOf(const std::string &filename)
	{
		const std::size_t underscore = filename.find('_');
		if (underscore == 0 || underscore == std::string::npos || underscore + 1 >= filename.size() - 4)
			return std::nullopt;
		int version = 0;
		const char *end = filename.data() + underscore;
		const auto [stop, error] = std::from_chars(filename.data(), end, version);
		if (error != std::errc{} || stop != end)
			return std::nullopt;
		return version;
	}

	core::Result<std::string> readFile(const std::filesystem::path &path)
	{
		std::ifstream file(path, std::ios::binary);
		if (!file)
			return core::fail(core::ErrorCode::Internal, "cannot read " + path.string());
		return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
	}
}

core::Result<std::vector<pg::MigrationFile>> pg::listMigrations(const std::filesystem::path &directory)
{
	std::error_code error;
	std::filesystem::directory_iterator entries(directory, error);
	if (error)
		return core::fail(core::ErrorCode::Internal,
						  "cannot open migration directory " + directory.string() + ": " + error.message());
	std::vector<MigrationFile> files;
	for (const auto &entry : entries)
	{
		if (!entry.is_regular_file() || entry.path().extension() != ".sql")
			continue;
		const std::string name = entry.path().filename().string();
		const auto version = versionOf(name);
		if (!version)
			return core::fail(core::ErrorCode::Invalid,
							  "invalid migration file name '" + name + "' (expected NNN_name.sql)");
		files.push_back({*version, name, entry.path()});
	}
	std::ranges::sort(files, {}, &MigrationFile::version);
	for (std::size_t i = 1; i < files.size(); ++i)
		if (files[i].version == files[i - 1].version)
			return core::fail(core::ErrorCode::Invalid,
							  "migrations " + files[i - 1].name + " and " + files[i].name + " have the same version");
	return files;
}

core::Result<pg::MigrationReport> pg::migrate(ConnectionPool &pool, const MigrationOptions &options,
											  core::ILogger &logger)
{
	if (!isPlainIdentifier(options.table))
		return core::fail(core::ErrorCode::Invalid, "invalid migrations table name '" + options.table + "'");
	const auto files = listMigrations(options.directory);
	if (!files)
		return std::unexpected(files.error());

	auto tx = pool.begin();
	if (!tx)
		return std::unexpected(tx.error());
	// Un seul migrateur à la fois. Le verrou est rendu tout seul au COMMIT ou au ROLLBACK.
	if (const auto locked = tx->query("SELECT pg_advisory_xact_lock(hashtextextended($1, 0))", options.table); !locked)
		return std::unexpected(locked.error());

	const std::string &table = options.table;
	if (const auto created = tx->execute("CREATE TABLE IF NOT EXISTS " + table + " ("
																				 "version INTEGER PRIMARY KEY, "
																				 "applied_at TIMESTAMPTZ NOT NULL DEFAULT now())");
		!created)
		return std::unexpected(created.error());
	// Les tables créées avant les sommes de contrôle n'ont pas cette colonne.
	if (const auto altered = tx->execute("ALTER TABLE " + table + " ADD COLUMN IF NOT EXISTS checksum TEXT"); !altered)
		return std::unexpected(altered.error());

	const auto rows = tx->execute("SELECT version, checksum FROM " + table);
	if (!rows)
		return std::unexpected(rows.error());
	std::map<int, std::optional<std::string>> applied;
	for (const Row row : *rows)
		applied[row.get<int>("version")] = row.get<std::optional<std::string>>("checksum");

	MigrationReport report;
	for (const MigrationFile &file : *files)
	{
		const auto sql = readFile(file.path);
		if (!sql)
			return std::unexpected(sql.error());
		// La base calcule le SHA-256 : pas de dépendance à une lib de hachage.
		const auto hashed = tx->query("SELECT encode(sha256(convert_to($1, 'UTF8')), 'hex') AS checksum", *sql);
		if (!hashed)
			return std::unexpected(hashed.error());
		const auto checksum = (*hashed)[0].get<std::string>("checksum");

		if (const auto known = applied.find(file.version); known != applied.end())
		{
			const std::optional<std::string> recordedChecksum = known->second;
			applied.erase(known); // ce qui restera : des versions appliquées sans fichier
			if (!recordedChecksum)
			{
				// Appliquée avant que les sommes de contrôle existent : on l'adopte.
				const auto recorded = tx->query("UPDATE " + table + " SET checksum = $2 WHERE version = $1",
												file.version, checksum);
				if (!recorded)
					return std::unexpected(recorded.error());
				logger.info("Migration checksum recorded", {{"migration", file.name}});
			}
			else if (*recordedChecksum != checksum)
				return core::fail(core::ErrorCode::Invalid,
								  "migration " + file.name + " was modified after being applied");
			++report.upToDate;
			continue;
		}

		logger.info(options.dryRun ? "Migration would be applied" : "Applying migration",
					{{"migration", file.name}});
		if (const auto ran = tx->executeScript(*sql); !ran)
			return core::fail(ran.error().code, "migration " + file.name + " failed: " + ran.error().message);
		const auto recorded = tx->query("INSERT INTO " + table + " (version, checksum) VALUES ($1, $2)",
										file.version, checksum);
		if (!recorded)
			return std::unexpected(recorded.error());
		report.applied.push_back(file.name);
	}
	for (const auto &[version, checksum] : applied)
		logger.warning("Applied migration has no file", {{"version", std::to_string(version)}});

	if (options.dryRun)
		return report; // le destructeur de tx fait ROLLBACK
	if (const auto committed = tx->commit(); !committed)
		return std::unexpected(committed.error());
	return report;
}