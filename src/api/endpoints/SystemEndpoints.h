#pragma once

#include <pistache/router.h>
#include <pistache/http.h>

#include <thread>
#include <rapidjson/document.h>

#include "services/SystemService.h"
#include "utils/JsonUtils.h"
#include "utils/ErrorHandler.h"
#include "utils/Exceptions.h"
#include "utils/HandlerWrapper.h"

class SystemEndpoints {
public:
	static void registerRoutes(Pistache::Rest::Router &router);

private:
	static void addResponseHeaders(Pistache::Http::ResponseWriter &response,
	                               bool isJson = true);

	static Pistache::Rest::Route::Result handleReady(
			const Pistache::Rest::Request &request,
			Pistache::Http::ResponseWriter response);

	static Pistache::Rest::Route::Result doDebug(
			const Pistache::Rest::Request &request,
			Pistache::Http::ResponseWriter response);

	static Pistache::Rest::Route::Result getInstance(
			const Pistache::Rest::Request &request,
			Pistache::Http::ResponseWriter response);

	static Pistache::Rest::Route::Result doShutdown(
			const Pistache::Rest::Request &request,
			Pistache::Http::ResponseWriter response);

	static Pistache::Rest::Route::Result executeCommand(
			const Pistache::Rest::Request &request,
			Pistache::Http::ResponseWriter response);

	static Pistache::Rest::Route::Result executePhysiologyModification(
			const Pistache::Rest::Request &request,
			Pistache::Http::ResponseWriter response);

	static Pistache::Rest::Route::Result executeRenderModification(
			const Pistache::Rest::Request &request,
			Pistache::Http::ResponseWriter response);
};