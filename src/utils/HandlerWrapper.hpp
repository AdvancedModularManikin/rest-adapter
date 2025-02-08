#pragma once

#include <pistache/router.h>
#include <pistache/http.h>
#include "utils/Exceptions.hpp"
#include "plog/Log.h"

class HandlerWrapper {
public:
	template<typename Handler>
	static auto WrapHandler(Handler handler) {
		return [handler](const Pistache::Rest::Request& request,
		                 Pistache::Http::ResponseWriter response) {
			try {
				return handler(request, std::move(response));
			}
			catch (const ValidationException& e) {
				response.headers()
						.add<Pistache::Http::Header::AccessControlAllowOrigin>("*")
						.add<Pistache::Http::Header::AccessControlAllowHeaders>(
								"Content-Type");
				response.send(Pistache::Http::Code::Bad_Request, e.what());
				return Pistache::Rest::Route::Result::Ok;
			}
			catch (const ResourceNotFoundException& e) {
				response.headers()
						.add<Pistache::Http::Header::AccessControlAllowOrigin>("*")
						.add<Pistache::Http::Header::AccessControlAllowHeaders>(
								"Content-Type");
				response.send(Pistache::Http::Code::Not_Found, e.what());
				return Pistache::Rest::Route::Result::Ok;
			}
			catch (const std::exception& e) {
				response.headers()
						.add<Pistache::Http::Header::AccessControlAllowOrigin>("*")
						.add<Pistache::Http::Header::AccessControlAllowHeaders>(
								"Content-Type");
				LOG_ERROR << "Unhandled exception: " << e.what();
				response.send(Pistache::Http::Code::Internal_Server_Error,
				              "Internal Server Error");
				return Pistache::Rest::Route::Result::Ok;
			}
		};
	}
};