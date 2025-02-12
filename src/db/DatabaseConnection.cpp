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

		// Verify table exists and has correct schema
		bool tableExists = false;
		(*m_db) << "SELECT count(*) FROM sqlite_master WHERE type='table' AND name='module_capabilities';"
		        >> tableExists;

		if (!tableExists) {
			LOG_ERROR << "module_capabilities table does not exist after migration";
			throw DatabaseException("Table initialization failed: module_capabilities table not found");
		}

		// Try to get table info
		(*m_db) << "PRAGMA table_info(module_capabilities);";
		LOG_DEBUG << "module_capabilities table verified successfully";

	} catch (const sqlite::sqlite_exception& e) {
		LOG_ERROR << "Database initialization failed: " << e.what();
		LOG_ERROR << "SQL State: " << e.get_code();
		LOG_ERROR << "Extended Error Code: " << e.get_extended_code();
		LOG_ERROR << "Query: " << e.get_sql();
		throw DatabaseException("Database initialization failed: " + std::string(e.what()));
	}
}