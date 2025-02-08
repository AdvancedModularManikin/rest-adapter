#pragma once

#include <string>
#include <exception>
#include <functional>
#include <pistache/http.h>
#include <pistache/router.h>

#include "amm/BaseLogger.h"
#include "JsonUtils.h"
#include "Exceptions.h"
#include "CustomHeaders.h"

class ErrorHandler {
public:
	static void handleApiError(Pistache::Http::ResponseWriter& response,
	                           const std::exception& e,
	                           const std::string& context);

	static void handleDatabaseError(const std::exception& e,
	                                const std::string& operation);

	static auto createErrorHandlingMiddleware();

	template<typename T>
	static void validateRequest(const T& request,
	                            const std::function<bool(const T&)>& validator,
	                            const std::string& errorMessage);

private:
	struct ErrorResponse {
		Pistache::Http::Code code;
		std::string message;
	};

	static ErrorResponse createErrorResponse(const std::exception& e,
	                                         const std::string& context);

	static std::string sanitizeErrorMessage(const std::string& message);
	static void addSecurityHeaders(Pistache::Http::ResponseWriter& response);

	ErrorHandler() = delete;
	~ErrorHandler() = delete;
	ErrorHandler(const ErrorHandler&) = delete;
	ErrorHandler& operator=(const ErrorHandler&) = delete;
};

// Template implementation needs to stay in header
template<typename T>
void ErrorHandler::validateRequest(const T& request,
                                   const std::function<bool(const T&)>& validator,
                                   const std::string& errorMessage) {
	if (!validator(request)) {
		throw ValidationException(errorMessage);
	}
}