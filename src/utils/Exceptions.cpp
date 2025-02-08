#include "Exceptions.h"

MoHSESException::MoHSESException(const std::string& message)
		: std::runtime_error(message) {}

FileException::FileException(const std::string& message)
		: std::runtime_error("File operation error: " + message) {}

DatabaseException::DatabaseException(const std::string& message)
		: std::runtime_error("Database error: " + message) {}

QueryException::QueryException(const std::string& message)
		: DatabaseException("Query error: " + message) {}

ConnectionException::ConnectionException(const std::string& message)
		: DatabaseException("Connection error: " + message) {}

MoHSESManagerException::MoHSESManagerException(const std::string& message)
		: MoHSESException("MoHSES Manager error: " + message) {}

ConfigException::ConfigException(const std::string& message)
		: std::runtime_error("Configuration error: " + message) {}

ApiException::ApiException(const std::string& message)
		: std::runtime_error("API error: " + message) {}

ValidationException::ValidationException(const std::string& message)
		: ApiException("Validation error: " + message) {}

ResourceNotFoundException::ResourceNotFoundException(const std::string& message)
		: ApiException("Resource not found: " + message) {}