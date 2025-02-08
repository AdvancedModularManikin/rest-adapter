#pragma once

#include <string>
#include <filesystem>

#include "core/Config.hpp"
#include "utils/Exceptions.hpp"

class DatabaseConfig {
public:
	static DatabaseConfig& getInstance() {
		static DatabaseConfig instance;
		return instance;
	}

	void initialize(const std::string& dbPath = "") {
		m_dbPath = dbPath.empty() ?
		           Config::getInstance().getDatabasePath() :
		           dbPath;
	}

	std::string getConnectionString() const {
		return m_dbPath;
	}

	std::string getMigrationPath() const {
		return (std::filesystem::path(Config::getInstance().getConfigPath()) / "migrations").string();
	}

private:
	DatabaseConfig() = default;
	std::string m_dbPath;


};