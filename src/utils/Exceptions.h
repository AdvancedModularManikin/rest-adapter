#pragma once

#include <stdexcept>
#include <string>

// Base exception class for our application
class MoHSESException : public std::runtime_error {
public:
	explicit MoHSESException(const std::string& message);
};

// File operation exceptions
class FileException : public std::runtime_error {
public:
	explicit FileException(const std::string& message);
};

// Database exceptions
class DatabaseException : public std::runtime_error {
public:
	explicit DatabaseException(const std::string& message);
};

class QueryException : public DatabaseException {
public:
	explicit QueryException(const std::string& message);
};

class ConnectionException : public DatabaseException {
public:
	explicit ConnectionException(const std::string& message);
};

// MoHSES Manager exceptions
class MoHSESManagerException : public MoHSESException {
public:
	explicit MoHSESManagerException(const std::string& message);
};

// Configuration exceptions
class ConfigException : public std::runtime_error {
public:
	explicit ConfigException(const std::string& message);
};

// API exceptions
class ApiException : public std::runtime_error {
public:
	explicit ApiException(const std::string& message);
};

class ValidationException : public ApiException {
public:
	explicit ValidationException(const std::string& message);
};

class ResourceNotFoundException : public ApiException {
public:
	explicit ResourceNotFoundException(const std::string& message);
};