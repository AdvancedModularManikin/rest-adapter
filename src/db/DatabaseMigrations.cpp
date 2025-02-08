#include "DatabaseMigrations.h"
#include "DatabaseConnection.h"

void DatabaseMigrations::runMigrations(DatabaseConnection& db) {
	try {
		createMigrationsTable(db);
		auto migrations = loadMigrations();
		executeMigrations(db, migrations);
	}
	catch (const std::exception& e) {
		LOG_ERROR << "Migration failed: " << e.what();
		throw DatabaseException("Failed to run migrations: " + std::string(e.what()));
	}
}

void DatabaseMigrations::createMigrationsTable(DatabaseConnection& db) {
	const char* sql = R"(
        CREATE TABLE IF NOT EXISTS schema_migrations (
            version INTEGER PRIMARY KEY,
            name TEXT NOT NULL,
            applied_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";

	db.execute(sql);
}

std::vector<DatabaseMigrations::Migration> DatabaseMigrations::loadMigrations() {
	std::vector<Migration> migrations;
	auto migrationPath = DatabaseConfig::getInstance().getMigrationPath();

	if (!std::filesystem::exists(migrationPath)) {
		LOG_WARNING << "Migrations directory not found at: " << migrationPath;
		return migrations;
	}

	std::regex migrationPattern(R"(^V(\d+)__(.+)\.sql$)");

	for (const auto& entry : std::filesystem::directory_iterator(migrationPath)) {
		if (entry.path().extension() == ".sql") {
			std::smatch matches;
			std::string filename = entry.path().filename().string();

			if (std::regex_match(filename, matches, migrationPattern)) {
				Migration migration;
				migration.version = std::stoi(matches[1].str());
				migration.name = matches[2].str();
				migration.sql = FileUtils::readFile(entry.path());
				migrations.push_back(migration);
			}
		}
	}

	std::sort(migrations.begin(), migrations.end(),
	          [](const Migration& a, const Migration& b) {
		          return a.version < b.version;
	          });

	return migrations;
}

void DatabaseMigrations::executeMigrations(DatabaseConnection& db,
                                           const std::vector<Migration>& migrations) {
	for (const auto& migration : migrations) {
		if (!isMigrationApplied(db, migration.version)) {
			LOG_INFO << "Applying migration V" << migration.version
			         << "__" << migration.name;

			db.executeTransaction([&](sqlite::database& transaction) {
				// Execute migration
				transaction << migration.sql;

				// Record migration
				transaction << "INSERT INTO schema_migrations (version, name) VALUES (?, ?)"
				            << migration.version
				            << migration.name;
			});

			LOG_INFO << "Migration V" << migration.version
			         << " applied successfully";
		}
	}
}

bool DatabaseMigrations::isMigrationApplied(DatabaseConnection& db, int version) {
	bool exists = false;
	try {
		db.executeQuery(
				"SELECT COUNT(*) FROM schema_migrations WHERE version = ?",
				[&exists](int count) {
					exists = (count > 0);
				}, version);
	}
	catch (const std::exception& e) {
		LOG_ERROR << "Error checking migration status: " << e.what();
		throw DatabaseException("Failed to check migration status: " + std::string(e.what()));
	}
	return exists;
}