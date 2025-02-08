#pragma once

#include <string>

class DatabaseConfig {
public:
	static DatabaseConfig& getInstance();

	void initialize(const std::string& dbPath = "");
	std::string getConnectionString() const;
	std::string getMigrationPath() const;

private:
	DatabaseConfig() = default;
	DatabaseConfig(const DatabaseConfig&) = delete;
	DatabaseConfig& operator=(const DatabaseConfig&) = delete;

	std::string m_dbPath;
};