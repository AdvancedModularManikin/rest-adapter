#pragma once

#include <string>
#include <exception>
#include <typeinfo>
#include <pistache/http.h>
#include <pistache/router.h>
#include <pistache/endpoint.h>

#include "amm/BaseLogger.h"

#include "JsonUtils.hpp"
#include "Exceptions.hpp"

namespace Pistache {
	namespace Http {
		namespace Header {
			class XFrameOptions : public Header {
			public:
				NAME("X-Frame-Options")

				XFrameOptions(const std::string& value)
						: value_(value) { }

				void write(std::ostream& os) const {
					os << value_;
				}

			private:
				std::string value_;
			};

			class XContentTypeOptions : public Header {
			public:
				NAME("X-Content-Type-Options")

				XContentTypeOptions(const std::string& value)
						: value_(value) { }

				void write(std::ostream& os) const {
					os << value_;
				}

			private:
				std::string value_;
			};

			class XXSSProtection : public Header {
			public:
				NAME("X-XSS-Protection")

				XXSSProtection(const std::string& value)
						: value_(value) { }

				void write(std::ostream& os) const {
					os << value_;
				}

			private:
				std::string value_;
			};
		}
	}
}


class ErrorHandler {
public:
	static void handleApiError(Pistache::Http::ResponseWriter& response,
	                           const std::exception& e,
	                           const std::string& context) {
		LOG_ERROR << context << ": " << e.what();

		auto errorResponse = createErrorResponse(e, context);

		addSecurityHeaders(response);

		response.send(errorResponse.code, errorResponse.message);
	}

	static void handleDatabaseError(const std::exception& e,
	                                const std::string& operation) {
		LOG_ERROR << "Database error during " << operation << ": " << e.what();
	}


private:
	struct ErrorResponse {
		Pistache::Http::Code code;
		std::string message;
	};

	static ErrorResponse createErrorResponse(const std::exception& e,
	                                         const std::string& context) {
		// Use typeid to check the exact type of the exception
		const std::type_info& type = typeid(e);

		if (type == typeid(ValidationException)) {
			return {
					Pistache::Http::Code::Bad_Request,
					JsonUtils::serializeError("Validation Error", e.what())
			};
		}
		else if (type == typeid(ResourceNotFoundException)) {
			return {
					Pistache::Http::Code::Not_Found,
					JsonUtils::serializeError("Not Found", e.what())
			};
		}
		else if (type == typeid(JsonUtils::ParseException)) {
			return {
					Pistache::Http::Code::Bad_Request,
					JsonUtils::serializeError("Invalid JSON", e.what())
			};
		}
		else if (type == typeid(FileException)) {
			return {
					Pistache::Http::Code::Internal_Server_Error,
					JsonUtils::serializeError("File Operation Error", e.what())
			};
		}
		else if (type == typeid(DatabaseException)) {
			return {
					Pistache::Http::Code::Internal_Server_Error,
					JsonUtils::serializeError("Database Error", "An internal database error occurred")
			};
		}
		else if (type == typeid(MoHSESManagerException)) {
			return {
					Pistache::Http::Code::Internal_Server_Error,
					JsonUtils::serializeError("MoHSES Manager Error", e.what())
			};
		}
		else {
			// Generic error handling for unknown exception types
			return {
					Pistache::Http::Code::Internal_Server_Error,
					JsonUtils::serializeError(
							"Internal Server Error",
							"An unexpected error occurred while processing your request"
					)
			};
		}
	}

	// Helper method to sanitize error messages for production use
	static std::string sanitizeErrorMessage(const std::string& message) {
		if (message.find("password") != std::string::npos ||
		    message.find("credential") != std::string::npos ||
		    message.find("token") != std::string::npos) {
			return "Internal server error";
		}
		return message;
	}

	// Middleware for adding common security headers
	static void addSecurityHeaders(Pistache::Http::ResponseWriter& response) {
		response.headers()
				.add<Pistache::Http::Header::AccessControlAllowOrigin>("*")
				.add<Pistache::Http::Header::XFrameOptions>("DENY")
				.add<Pistache::Http::Header::XContentTypeOptions>("nosniff")
				.add<Pistache::Http::Header::XXSSProtection>("1; mode=block");
	}

public:
	// Middleware factory for error handling
	static auto createErrorHandlingMiddleware() {
		return [](const Pistache::Http::Request& request,
		          Pistache::Http::ResponseWriter response) {
			try {
				response.send(Pistache::Http::Code::Ok);
			}
			catch (const std::exception& e) {
				handleApiError(response, e, "Middleware error handling");
			}
		};
	}

	// Request validation helper
	template<typename T>
	static void validateRequest(const T& request,
	                            const std::function<bool(const T&)>& validator,
	                            const std::string& errorMessage) {
		if (!validator(request)) {
			throw ValidationException(errorMessage);
		}
	}

	// Rate limiting helper
	static void checkRateLimit(const std::string& clientId,
	                           const std::string& endpoint) {
		// Implement rate limiting logic here
		LOG_INFO << "Rate limit check for client " << clientId
		         << " on endpoint " << endpoint;
	}

private:

	ErrorHandler() = delete;
	~ErrorHandler() = delete;
	ErrorHandler(const ErrorHandler&) = delete;
	ErrorHandler& operator=(const ErrorHandler&) = delete;
};