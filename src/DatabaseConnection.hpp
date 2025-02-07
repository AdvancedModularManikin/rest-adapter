// DatabaseConnection.hpp
#pragma once

#include <memory>
#include <string>
#include <stdexcept>
#include <functional>
#include <sstream>
#include <chrono>
#include <optional>
#include <vector>
#include <mutex>
#include <shared_mutex>

#include "thirdparty/sqlite_modern_cpp.h"
#include "plog/Log.h"

// Custom exception classes
class DatabaseException : public std::runtime_error {
public:
	explicit DatabaseException(const std::string& message)
			: std::runtime_error(message) {}
};

class QueryException : public DatabaseException {
public:
	explicit QueryException(const std::string& message)
			: DatabaseException(message) {}
};

class ConnectionException : public DatabaseException {
public:
	explicit ConnectionException(const std::string& message)
			: DatabaseException(message) {}
};

class DatabaseConnection {
public:
	// Constructor with connection string
	explicit DatabaseConnection(const std::string& connectionString)
	try : m_db(std::make_unique<sqlite::database>(connectionString)) {
		LOG_INFO << "Initializing database connection to: " << connectionString;
		initializeDatabase();
	} catch (const sqlite::sqlite_exception& e) {
		LOG_ERROR << "Failed to create database connection: " << e.what();
		throw ConnectionException("Failed to create database connection: " + std::string(e.what()));
	}

	// Deleted copy constructor and assignment operator
	DatabaseConnection(const DatabaseConnection&) = delete;
	DatabaseConnection& operator=(const DatabaseConnection&) = delete;

	// Move constructor
	DatabaseConnection(DatabaseConnection&& other) noexcept
			: m_db(std::move(other.m_db)) {}

	// Move assignment operator
	DatabaseConnection& operator=(DatabaseConnection&& other) noexcept {
		if (this != &other) {
			m_db = std::move(other.m_db);
		}
		return *this;
	}

	// Destructor
	~DatabaseConnection() {
		try {
			if (m_db) {
				// Ensure all transactions are finished
				execute("COMMIT;");
			}
		} catch (const std::exception& e) {
			LOG_ERROR << "Error during database cleanup: " << e.what();
		}
	}

	// Get database reference
	sqlite::database& get() {
		if (!m_db) {
			throw ConnectionException("Database connection is null");
		}
		return *m_db;
	}

	sqlite::database& db() {
		if (!m_db) {
			throw ConnectionException("Database connection is null");
		}
		return *m_db;
	}

	// Execute a query with parameters
	template<typename... Args>
	void executeQuery(const std::string& query, const std::function<void(sqlite::database_binder&&)>& callback, Args&&... args) {
		try {
			std::lock_guard<std::mutex> lock(m_queryMutex);
			auto binder = (*m_db) << query;
			(binder << ... << std::forward<Args>(args));
			callback(std::move(binder));
		} catch (const sqlite::sqlite_exception& e) {
			LOG_ERROR << "Query execution failed: " << e.what() << " Query: " << query;
			throw QueryException("Query execution failed: " + std::string(e.what()));
		}
	}
	template<typename... Args>
	void execute(const std::string& query, Args&&... args) {
		try {
			std::lock_guard<std::mutex> lock(m_queryMutex);
			auto binder = (*m_db) << query;
			(binder << ... << std::forward<Args>(args));
			binder.execute();
		} catch (const sqlite::sqlite_exception& e) {
			LOG_ERROR << "Query execution failed: " << e.what() << " Query: " << query;
			throw QueryException("Query execution failed: " + std::string(e.what()));
		}
	}

	// Transaction wrapper
	template<typename Func>
	void executeTransaction(Func&& func) {
		std::lock_guard<std::mutex> lock(m_queryMutex);
		try {
			(*m_db) << "BEGIN TRANSACTION;";
			func(get());
			(*m_db) << "COMMIT;";
		} catch (...) {
			try {
				(*m_db) << "ROLLBACK;";
				LOG_WARNING << "Transaction rolled back due to error";
			} catch (const std::exception& e) {
				LOG_ERROR << "Failed to rollback transaction: " << e.what();
			}
			throw;
		}
	}

	// Check if connection is valid
	bool isValid() const noexcept {
		return m_db != nullptr;
	}

	// Get the last inserted row id
	int64_t getLastInsertId() const {
		try {
			int64_t id = 0;
			(*m_db) << "SELECT last_insert_rowid();" >> id;
			return id;
		} catch (const sqlite::sqlite_exception& e) {
			LOG_ERROR << "Failed to get last insert id: " << e.what();
			throw QueryException("Failed to get last insert id: " + std::string(e.what()));
		}
	}

private:
	std::unique_ptr<sqlite::database> m_db;
	mutable std::mutex m_queryMutex;

	void initializeDatabase() {
		try {
			executeTransaction([this](sqlite::database& db) {
				// Create module_capabilities table
				execute(R"(
                    CREATE TABLE IF NOT EXISTS module_capabilities (
                        module_id TEXT PRIMARY KEY,
                        module_guid TEXT UNIQUE,
                        module_name TEXT NOT NULL,
                        description TEXT,
                        capabilities TEXT,
                        manufacturer TEXT,
                        model TEXT,
                        created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                        updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
                    )
                )");

				// Create events table
				execute(R"(
                    CREATE TABLE IF NOT EXISTS events (
                        id INTEGER PRIMARY KEY AUTOINCREMENT,
                        source TEXT,
                        topic TEXT,
                        timestamp INTEGER,
                        tick INTEGER,
                        data TEXT,
                        created_at DATETIME DEFAULT CURRENT_TIMESTAMP
                    )
                )");

				// Create logs table
				execute(R"(
                    CREATE TABLE IF NOT EXISTS logs (
                        id INTEGER PRIMARY KEY AUTOINCREMENT,
                        module_name TEXT,
                        module_guid TEXT,
                        module_id TEXT,
                        message TEXT,
                        log_level TEXT,
                        timestamp INTEGER,
                        created_at DATETIME DEFAULT CURRENT_TIMESTAMP
                    )
                )");

				// Create indexes
				execute("CREATE INDEX IF NOT EXISTS idx_events_timestamp ON events(timestamp)");
				execute("CREATE INDEX IF NOT EXISTS idx_events_source ON events(source)");
				execute("CREATE INDEX IF NOT EXISTS idx_logs_timestamp ON logs(timestamp)");
				execute("CREATE INDEX IF NOT EXISTS idx_logs_module ON logs(module_name)");
				execute("CREATE INDEX IF NOT EXISTS idx_capabilities_name ON module_capabilities(module_name)");

				// Create trigger for updated_at
				execute(R"(
                    CREATE TRIGGER IF NOT EXISTS update_module_capabilities_timestamp 
                    AFTER UPDATE ON module_capabilities
                    BEGIN
                        UPDATE module_capabilities 
                        SET updated_at = CURRENT_TIMESTAMP 
                        WHERE module_id = NEW.module_id;
                    END
                )");

			});

			LOG_INFO << "Database initialization completed successfully";
		} catch (const sqlite::sqlite_exception& e) {
			LOG_ERROR << "Database initialization failed: " << e.what();
			throw DatabaseException("Database initialization failed: " + std::string(e.what()));
		}
	}

	// Helper method to check if table exists
	bool tableExists(const std::string& tableName) {
		try {
			bool exists = false;
			(*m_db) << "SELECT count(*) FROM sqlite_master WHERE type='table' AND name=?"
			        << tableName >> exists;
			return exists;
		} catch (const sqlite::sqlite_exception& e) {
			LOG_ERROR << "Failed to check if table exists: " << e.what();
			throw QueryException("Failed to check if table exists: " + std::string(e.what()));
		}
	}
};

// Connection pool for handling multiple database connections
class DatabaseConnectionPool {
public:
	static DatabaseConnectionPool& getInstance() {
		static DatabaseConnectionPool instance;
		return instance;
	}

	void initialize(const std::string& connectionString, size_t poolSize = 5) {
		std::lock_guard<std::mutex> lock(m_poolMutex);
		try {
			for (size_t i = 0; i < poolSize; ++i) {
				m_connections.push_back(std::make_unique<DatabaseConnection>(connectionString));
			}
			LOG_INFO << "Connection pool initialized with " << poolSize << " connections";
		} catch (const std::exception& e) {
			LOG_ERROR << "Failed to initialize connection pool: " << e.what();
			throw ConnectionException("Failed to initialize connection pool: " + std::string(e.what()));
		}
	}

	DatabaseConnection& getConnection() {
		std::lock_guard<std::mutex> lock(m_poolMutex);
		if (m_connections.empty()) {
			throw ConnectionException("No available connections in pool");
		}

		// Simple round-robin connection distribution
		auto& conn = m_connections[m_currentIndex];
		m_currentIndex = (m_currentIndex + 1) % m_connections.size();
		return *conn;
	}

private:
	DatabaseConnectionPool() = default;
	~DatabaseConnectionPool() = default;
	DatabaseConnectionPool(const DatabaseConnectionPool&) = delete;
	DatabaseConnectionPool& operator=(const DatabaseConnectionPool&) = delete;

	std::vector<std::unique_ptr<DatabaseConnection>> m_connections;
	std::mutex m_poolMutex;
	size_t m_currentIndex = 0;
};