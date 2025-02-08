#pragma once

#include <pistache/router.h>
#include <pistache/http.h>

#include <rapidjson/document.h>

#include "services/StatusService.h"
#include "utils/JsonUtils.h"
#include "utils/ErrorHandler.h"
#include "utils/Exceptions.h"
#include "utils/HandlerWrapper.h"

class StatusEndpoints {
public:
	static void registerRoutes(Pistache::Rest::Router& router);

private:
	static void addResponseHeaders(Pistache::Http::ResponseWriter& response,
	                               bool isJson = true);

	static Pistache::Rest::Route::Result getStatus(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response);

	static Pistache::Rest::Route::Result getStatusValue(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response);

	static Pistache::Rest::Route::Result getNodes(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response);

	static Pistache::Rest::Route::Result getNode(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response);

	static Pistache::Rest::Route::Result getLabsReport(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response);
};