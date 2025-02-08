#pragma once

#include <memory>
#include <string>
#include <mutex>
#include <functional>

#include "thirdparty/sqlite_modern_cpp.h"

#include "amm/BaseLogger.h"

#include "utils/Exceptions.h"
#include "DatabaseConfig.h"
#include "DatabaseMigrations.h"

class DatabaseConnection {
public:
	explicit DatabaseConnection(const std::string& connectionString = "");

	// Deleted copy constructor and assignment operator
	DatabaseConnection(const DatabaseConnection&) = delete;
	DatabaseConnection& operator=(const DatabaseConnection&) = delete;

	// Move constructor and assignment operator
	DatabaseConnection(DatabaseConnection&& other) noexcept = default;
	DatabaseConnection& operator=(DatabaseConnection&& other) noexcept = default;

	sqlite::database& db();

	// For queries without parameters
	template<typename Callback>
	void executeQuery(const std::string& query, Callback callback);

	// For queries with parameters
	template<typename Callback, typename Param>
	void executeQuery(const std::string& query, Callback callback, const Param& param);

	// For queries with multiple parameters
	template<typename Callback, typename... Params>
	void executeQuery(const std::string& query, Callback callback, const Params&... params);

	template<typename... Args>
	void execute(const std::string& query, Args&&... args);

	template<typename Func>
	void executeTransaction(Func&& func);

	int64_t getLastInsertId() const;

private:
	std::unique_ptr<sqlite::database> m_db;
	mutable std::mutex m_mutex;

	void initializeDatabase();
};

// Template implementations must remain in the header
template<typename Callback>
void DatabaseConnection::executeQuery(const std::string& query, Callback callback) {
	std::lock_guard<std::mutex> lock(m_mutex);
	try {
		(*m_db) << query >> callback;
	}
	catch (const sqlite::sqlite_exception& e) {
		LOG_ERROR << "Query execution failed: " << e.what()
		          << " Query: " << query;
		throw QueryException("Query execution failed: " + std::string(e.what()));
	}
}

template<typename Callback, typename Param>
void DatabaseConnection::executeQuery(const std::string& query, Callback callback, const Param& param) {
	std::lock_guard<std::mutex> lock(m_mutex);
	try {
		(*m_db) << query << param >> callback;
	}
	catch (const sqlite::sqlite_exception& e) {
		LOG_ERROR << "Query execution failed: " << e.what()
		          << " Query: " << query;
		throw QueryException("Query execution failed: " + std::string(e.what()));
	}
}

template<typename Callback, typename... Params>
void DatabaseConnection::executeQuery(const std::string& query, Callback callback, const Params&... params) {
	std::lock_guard<std::mutex> lock(m_mutex);
	try {
		(*m_db) << query << std::make_tuple(params...) >> callback;
	}
	catch (const sqlite::sqlite_exception& e) {
		LOG_ERROR << "Query execution failed: " << e.what()
		          << " Query: " << query;
		throw QueryException("Query execution failed: " + std::string(e.what()));
	}
}

template<typename... Args>
void DatabaseConnection::execute(const std::string& query, Args&&... args) {
	std::lock_guard<std::mutex> lock(m_mutex);
	try {
		auto binder = (*m_db) << query;
		(binder << ... << std::forward<Args>(args));
		binder.execute();
	}
	catch (const sqlite::sqlite_exception& e) {
		LOG_ERROR << "Query execution failed: " << e.what()
		          << " Query: " << query;
		throw QueryException("Query execution failed: " + std::string(e.what()));
	}
}

template<typename Func>
void DatabaseConnection::executeTransaction(Func&& func) {
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