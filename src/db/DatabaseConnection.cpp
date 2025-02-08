#include "DatabaseConnection.h"


DatabaseConnection::DatabaseConnection(const std::string& connectionString) {
	try {
		auto& config = DatabaseConfig::getInstance();
		if (!connectionString.empty()) {
			config.initialize(connectionString);
		}

		m_db = std::make_unique<sqlite::database>(
				config.getConnectionString()
		);

		initializeDatabase();
	}
	catch (const sqlite::sqlite_exception& e) {
		LOG_ERROR << "Failed to create database connection: " << e.what();
		throw ConnectionException("Failed to create database connection: " +
		                          std::string(e.what()));
	}
}

sqlite::database& DatabaseConnection::db() {
	if (!m_db) {
		throw ConnectionException("Database connection is null");
	}
	return *m_db;
}

int64_t DatabaseConnection::getLastInsertId() const {
	try {
		int64_t id = 0;
		(*m_db) << "SELECT last_insert_rowid();" >> id;
		return id;
	}
	catch (const sqlite::sqlite_exception& e) {
		LOG_ERROR << "Failed to get last insert id: " << e.what();
		throw QueryException("Failed to get last insert id: " +
		                     std::string(e.what()));
	}
}

void DatabaseConnection::initializeDatabase() {
	try {
		// Run migrations
		DatabaseMigrations::runMigrations(*this);
	}
	catch (const sqlite::sqlite_exception& e) {
		LOG_ERROR << "Database initialization failed: " << e.what();
		throw DatabaseException("Database initialization failed: " +
		                        std::string(e.what()));
	}
}