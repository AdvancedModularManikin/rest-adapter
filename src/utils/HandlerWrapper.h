#pragma once

#include <pistache/router.h>
#include <pistache/http.h>

#include "utils/Exceptions.h"
#include "amm/BaseLogger.h"

class HandlerWrapper {
public:
	template<typename Handler>
	static auto WrapHandler(Handler handler);

private:
	static void addCorsHeaders(Pistache::Http::ResponseWriter& response);
};

template<typename Handler>
auto HandlerWrapper::WrapHandler(Handler handler) {
	return [handler](const Pistache::Rest::Request& request,
	                 Pistache::Http::ResponseWriter response) {
		try {
			return handler(request, std::move(response));
		}
		catch (const ValidationException& e) {
			addCorsHeaders(response);
			response.send(Pistache::Http::Code::Bad_Request, e.what());
			return Pistache::Rest::Route::Result::Ok;
		}
		catch (const ResourceNotFoundException& e) {
			addCorsHeaders(response);
			response.send(Pistache::Http::Code::Not_Found, e.what());
			return Pistache::Rest::Route::Result::Ok;
		}
		catch (const std::exception& e) {
			addCorsHeaders(response);
			LOG_ERROR << "Unhandled exception: " << e.what();
			response.send(Pistache::Http::Code::Internal_Server_Error,
			              "Internal Server Error");
			return Pistache::Rest::Route::Result::Ok;
		}
	};
}