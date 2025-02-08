#include "DatabaseConfig.h"
#include <filesystem>
#include "core/Config.h"
#include "utils/Exceptions.h"

DatabaseConfig& DatabaseConfig::getInstance() {
	static DatabaseConfig instance;
	return instance;
}

void DatabaseConfig::initialize(const std::string& dbPath) {
	m_dbPath = dbPath.empty() ?
	           Config::getInstance().getDatabasePath() :
	           dbPath;
}

std::string DatabaseConfig::getConnectionString() const {
	return m_dbPath;
}

std::string DatabaseConfig::getMigrationPath() const {
	return (std::filesystem::path(Config::getInstance().getConfigPath()) / "migrations").string();
}