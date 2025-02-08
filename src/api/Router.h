#pragma once

#include <memory>

#include <pistache/router.h>
#include <pistache/endpoint.h>

#include "utils/ErrorHandler.h"

#include "endpoints/ActionEndpoints.h"
#include "endpoints/AssessmentEndpoints.h"
#include "endpoints/ModuleEndpoints.h"
#include "endpoints/StatusEndpoints.h"
#include "endpoints/SystemEndpoints.h"

class Router {
public:
	Router() = default;
	void initializeRoutes(Pistache::Http::Endpoint& endpoint);

private:
	std::unique_ptr<Pistache::Rest::Router> m_router;

	void setupCORSHandlers();
	void registerEndpoints();

	static Pistache::Rest::Route::Result handleCorsOptions(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response);

	static Pistache::Rest::Route::Result handleCors(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response);

	static Pistache::Rest::Route::Result handleNotFound(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response);
};