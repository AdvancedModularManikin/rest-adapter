#pragma once

#include <memory>
#include <string>
#include <mutex>
#include <functional>

#include "amm/BaseLogger.h"

#include "thirdparty/sqlite_modern_cpp.h"

#include "utils/Exceptions.hpp"

#include "DatabaseConfig.hpp"
#include "DatabaseMigrations.h"

class DatabaseConnection {
public:
	explicit DatabaseConnection(const std::string& connectionString = "") {
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

	// Deleted copy constructor and assignment operator
	DatabaseConnection(const DatabaseConnection&) = delete;
	DatabaseConnection& operator=(const DatabaseConnection&) = delete;

	// Move constructor
	DatabaseConnection(DatabaseConnection&& other) noexcept = default;

	// Move assignment operator
	DatabaseConnection& operator=(DatabaseConnection&& other) noexcept = default;

	sqlite::database& db() {
		if (!m_db) {
			throw ConnectionException("Database connection is null");
		}
		return *m_db;
	}

	// For queries without parameters
	template<typename Callback>
	void executeQuery(const std::string& query, Callback callback) {
		std::lock_guard<std::mutex> lock(m_mutex);
		try {
			(*m_db) << query >> callback;
		}
		catch (const sqlite::sqlite_exception& e) {
			LOG_ERROR << "Query execution failed: " << e.what()
			          << " Query: " << query;
			throw QueryException("Query execution failed: " +
			                     std::string(e.what()));
		}
	}

	// For queries with parameters
	template<typename Callback, typename Param>
	void executeQuery(const std::string& query, Callback callback, const Param& param) {
		std::lock_guard<std::mutex> lock(m_mutex);
		try {
			(*m_db) << query << param >> callback;
		}
		catch (const sqlite::sqlite_exception& e) {
			LOG_ERROR << "Query execution failed: " << e.what()
			          << " Query: " << query;
			throw QueryException("Query execution failed: " +
			                     std::string(e.what()));
		}
	}

	// For queries with multiple parameters
	template<typename Callback, typename... Params>
	void executeQuery(const std::string& query, Callback callback,
	                  const Params&... params) {
		std::lock_guard<std::mutex> lock(m_mutex);
		try {
			(*m_db) << query << std::make_tuple(params...) >> callback;
		}
		catch (const sqlite::sqlite_exception& e) {
			LOG_ERROR << "Query execution failed: " << e.what()
			          << " Query: " << query;
			throw QueryException("Query execution failed: " +
			                     std::string(e.what()));
		}
	}

	template<typename... Args>
	void execute(const std::string& query, Args&&... args) {
		std::lock_guard<std::mutex> lock(m_mutex);
		try {
			auto binder = (*m_db) << query;
			(binder << ... << std::forward<Args>(args));
			binder.execute();
		}
		catch (const sqlite::sqlite_exception& e) {
			LOG_ERROR << "Query execution failed: " << e.what()
			          << " Query: " << query;
			throw QueryException("Query execution failed: " +
			                     std::string(e.what()));
		}
	}

	template<typename Func>
	void executeTransaction(Func&& func) {
		std::lock_guard<std::mutex> lock(m_mutex);
		try {
			(*m_db) << "BEGIN TRANSACTION;";
			func(*m_db);
			(*m_db) << "COMMIT;";
		}
		catch (...) {
			try {
				(*m_db) << "ROLLBACK;";
				LOG_WARNING << "Transaction rolled back due to error";
			}
			catch (const std::exception& e) {
				LOG_ERROR << "Failed to rollback transaction: " << e.what();
			}
			throw;
		}
	}

	int64_t getLastInsertId() const {
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

private:
	std::unique_ptr<sqlite::database> m_db;
	mutable std::mutex m_mutex;

	void initializeDatabase() {
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
};