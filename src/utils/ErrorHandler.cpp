#include "ErrorHandler.h"


void ErrorHandler::handleApiError(Pistache::Http::ResponseWriter& response,
                                  const std::exception& e,
                                  const std::string& context) {
	LOG_ERROR << context << ": " << e.what();

	auto errorResponse = createErrorResponse(e, context);
	addSecurityHeaders(response);
	response.send(errorResponse.code, errorResponse.message);
}

void ErrorHandler::handleDatabaseError(const std::exception& e,
                                       const std::string& operation) {
	LOG_ERROR << "Database error during " << operation << ": " << e.what();
}

auto ErrorHandler::createErrorHandlingMiddleware() {
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

ErrorHandler::ErrorResponse ErrorHandler::createErrorResponse(
		const std::exception& e,
		const std::string& context) {
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
		return {
				Pistache::Http::Code::Internal_Server_Error,
				JsonUtils::serializeError(
						"Internal Server Error",
						"An unexpected error occurred while processing your request"
				)
		};
	}
}

std::string ErrorHandler::sanitizeErrorMessage(const std::string& message) {
	if (message.find("password") != std::string::npos ||
	    message.find("credential") != std::string::npos ||
	    message.find("token") != std::string::npos) {
		return "Internal server error";
	}
	return message;
}

void ErrorHandler::addSecurityHeaders(Pistache::Http::ResponseWriter& response) {
	response.headers()
			.add<Pistache::Http::Header::AccessControlAllowOrigin>("*")
			.add<Pistache::Http::Header::XFrameOptions>("DENY")
			.add<Pistache::Http::Header::XContentTypeOptions>("nosniff")
			.add<Pistache::Http::Header::XXSSProtection>("1; mode=block");
}