#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <regex>

#include "plog/Log.h"
#include "utils/FileUtils.hpp"
#include "utils/Exceptions.hpp"
#include "DatabaseConfig.hpp"

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
};