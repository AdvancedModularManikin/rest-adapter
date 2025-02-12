#include "DatabaseMigrations.h"
#include "DatabaseConnection.h"

void DatabaseMigrations::runMigrations(DatabaseConnection& db) {
	try {
		LOG_DEBUG << "Starting database migrations...";

		createMigrationsTable(db);

		auto migrationPath = DatabaseConfig::getInstance().getMigrationPath();
		LOG_DEBUG << "Looking for migrations in: " << migrationPath;

		auto migrations = loadMigrations();
		LOG_DEBUG << "Found " << migrations.size() << " migration(s)";

		if (migrations.empty()) {
			LOG_WARNING << "No migrations found to execute";
			return;
		}

		executeMigrations(db, migrations);
		LOG_DEBUG << "Database migrations completed successfully";
	}
	catch (const std::exception& e) {
		LOG_ERROR << "Migration failed: " << e.what();
		throw DatabaseException("Failed to run migrations: " + std::string(e.what()));
	}
}

void DatabaseMigrations::createMigrationsTable(DatabaseConnection& db) {
	LOG_DEBUG << "Creating or verifying schema_migrations table...";
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
		LOG_ERROR << "Migrations directory not found at: " << migrationPath;
		throw DatabaseException("Migrations directory not found: " + migrationPath);
	}

	std::regex migrationPattern(R"(^V(\d+)__(.+)\.sql$)");

	for (const auto& entry : std::filesystem::directory_iterator(migrationPath)) {
		if (entry.path().extension() == ".sql") {
			std::smatch matches;
			std::string filename = entry.path().filename().string();

			LOG_DEBUG << "Found SQL file: " << filename;

			if (std::regex_match(filename, matches, migrationPattern)) {
				Migration migration;
				migration.version = std::stoi(matches[1].str());
				migration.name = matches[2].str();
				migration.sql = FileUtils::readFile(entry.path());
				migrations.push_back(migration);
				LOG_DEBUG << "Added migration V" << migration.version << ": " << migration.name;
			} else {
				LOG_WARNING << "Skipping " << filename << " - doesn't match migration filename pattern";
			}
		}
	}

	std::sort(migrations.begin(), migrations.end(),
	          [](const Migration& a, const Migration& b) {
		          return a.version < b.version;
	          });

	return migrations;
}

std::vector<std::string> DatabaseMigrations::splitStatements(const std::string& sql) {
	std::vector<std::string> statements;
	std::string currentStatement;
	bool inTrigger = false;

	std::istringstream stream(sql);
	std::string line;

	while (std::getline(stream, line)) {
		// Skip empty lines
		if (line.empty() || std::all_of(line.begin(), line.end(), isspace)) {
			continue;
		}

		// Trim whitespace
		line.erase(0, line.find_first_not_of(" \t"));

		// Skip comment lines
		if (line.starts_with("--")) {
			continue;
		}

		// Check if we're entering a trigger definition
		if (line.find("CREATE TRIGGER") != std::string::npos) {
			inTrigger = true;
		}

		currentStatement += line + " ";

		// If we're in a trigger, wait for END; otherwise check for regular semicolon
		if (inTrigger) {
			if (line.find("END;") != std::string::npos) {
				inTrigger = false;
				statements.push_back(currentStatement);
				currentStatement.clear();
			}
		} else if (line.find(';') != std::string::npos) {
			statements.push_back(currentStatement);
			currentStatement.clear();
		}
	}

	// Add any remaining statement
	if (!currentStatement.empty()) {
		statements.push_back(currentStatement);
	}

	return statements;
}

void DatabaseMigrations::executeMigrations(DatabaseConnection& db,
                                           const std::vector<Migration>& migrations) {
	for (const auto& migration : migrations) {
		if (!isMigrationApplied(db, migration.version)) {
			LOG_DEBUG << "Applying migration V" << migration.version
			         << "__" << migration.name;

			try {
				db.executeTransaction([&](sqlite::database& transaction) {
					// Split and execute each statement separately
					auto statements = splitStatements(migration.sql);
					LOG_DEBUG << "Executing " << statements.size() << " statements for migration V"
					         << migration.version;

					for (const auto& statement : statements) {
						if (!statement.empty()) {
							LOG_DEBUG << "Executing statement: " << statement;
							transaction << statement;
						}
					}

					// Record migration
					LOG_DEBUG << "Recording migration in schema_migrations...";
					transaction << "INSERT INTO schema_migrations (version, name) VALUES (?, ?)"
					            << migration.version
					            << migration.name;
				});

				LOG_DEBUG << "Migration V" << migration.version
				         << " applied successfully";
			} catch (const std::exception& e) {
				LOG_ERROR << "Failed to apply migration V" << migration.version
				          << ": " << e.what();
				throw;
			}
		} else {
			LOG_DEBUG << "Migration V" << migration.version << " already applied, skipping";  // Fixed this line
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

		LOG_DEBUG << "Migration V" << version
		         << (exists ? " is already applied" : " is not yet applied");
	}
	catch (const std::exception& e) {
		LOG_ERROR << "Error checking migration status: " << e.what();
		throw DatabaseException("Failed to check migration status: " + std::string(e.what()));
	}
	return exists;
}