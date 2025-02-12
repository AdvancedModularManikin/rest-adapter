#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <algorithm>

#include "amm/BaseLogger.h"

#include "utils/FileUtils.h"
#include "utils/Exceptions.h"

#include "DatabaseConfig.h"

// Forward declaration
class DatabaseConnection;

class DatabaseMigrations {
public:
	static void runMigrations(DatabaseConnection& db);

private:
	struct Migration {
		int version;
		std::string name;
		std::string sql;
	};

	static void createMigrationsTable(DatabaseConnection& db);
	static std::vector<Migration> loadMigrations();
	static void executeMigrations(DatabaseConnection& db,
	                              const std::vector<Migration>& migrations);
	static bool isMigrationApplied(DatabaseConnection& db, int version);

	// New helper method for splitting SQL statements
	static std::vector<std::string> splitStatements(const std::string& sql);
};