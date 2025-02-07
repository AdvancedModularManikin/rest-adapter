#pragma once

#include "DatabaseConnection.hpp"

class ResourceManager {
public:
	static ResourceManager& getInstance() {
		static ResourceManager instance;
		return instance;
	}

	// Initialize all resources
	void initialize(const std::string& dbPath) {
		try {
			m_dbConnection = std::make_unique<DatabaseConnection>(dbPath);
			// Initialize other resources here
		} catch (const std::exception& e) {
			throw std::runtime_error("Failed to initialize resources: " + std::string(e.what()));
		}
	}

	// Get database connection
	DatabaseConnection& getDatabase() {
		if (!m_dbConnection) {
			throw std::runtime_error("Database connection not initialized");
		}
		return *m_dbConnection;
	}

	// Cleanup resources
	void cleanup() {
		m_dbConnection.reset();
		// Cleanup other resources
	}

private:
	ResourceManager() = default;
	~ResourceManager() {
		cleanup();
	}

	ResourceManager(const ResourceManager&) = delete;
	ResourceManager& operator=(const ResourceManager&) = delete;

	std::unique_ptr<DatabaseConnection> m_dbConnection;
};
