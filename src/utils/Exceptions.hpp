#pragma once

#include <stdexcept>
#include <string>

// Base exception class for our application
class MoHSESException : public std::runtime_error {
public:
	explicit MoHSESException(const std::string& message)
			: std::runtime_error(message) {}
};

// File operation exceptions
class FileException : public std::runtime_error {
public:
	explicit FileException(const std::string& message)
			: std::runtime_error("File operation error: " + message) {}
};

// Database exceptions
class DatabaseException : public std::runtime_error {
public:
	explicit DatabaseException(const std::string& message)
			: std::runtime_error("Database error: " + message) {}
};

class QueryException : public DatabaseException {
public:
	explicit QueryException(const std::string& message)
			: DatabaseException("Query error: " + message) {}
};

class ConnectionException : public DatabaseException {
public:
	explicit ConnectionException(const std::string& message)
			: DatabaseException("Connection error: " + message) {}
};

// MoHSES Manager exceptions
class MoHSESManagerException : public MoHSESException {
public:
	explicit MoHSESManagerException(const std::string& message)
			: MoHSESException("MoHSES Manager error: " + message) {}
};

// Configuration exceptions
class ConfigException : public std::runtime_error {
public:
	explicit ConfigException(const std::string& message)
			: std::runtime_error("Configuration error: " + message) {}
};

// API exceptions
class ApiException : public std::runtime_error {
public:
	explicit ApiException(const std::string& message)
			: std::runtime_error("API error: " + message) {}
};

class ValidationException : public ApiException {
public:
	explicit ValidationException(const std::string& message)
			: ApiException("Validation error: " + message) {}
};

class ResourceNotFoundException : public ApiException {
public:
	explicit ResourceNotFoundException(const std::string& message)
			: ApiException("Resource not found: " + message) {}
};

// Authentication/Authorization exceptions
class AuthenticationException : public ApiException {
public:
	explicit AuthenticationException(const std::string& message)
			: ApiException("Authentication error: " + message) {}
};

class AuthorizationException : public ApiException {
public:
	explicit AuthorizationException(const std::string& message)
			: ApiException("Authorization error: " + message) {}
};

// Rate limiting exceptions
class RateLimitException : public ApiException {
public:
	explicit RateLimitException(const std::string& message)
			: ApiException("Rate limit exceeded: " + message) {}
};