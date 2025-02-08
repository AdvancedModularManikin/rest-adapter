#pragma once

#include <pistache/router.h>
#include <pistache/http.h>

#include <rapidjson/document.h>

#include "services/ModuleService.h"
#include "utils/JsonUtils.h"
#include "utils/ErrorHandler.h"
#include "utils/Exceptions.h"
#include "utils/HandlerWrapper.h"

class ModuleEndpoints {
public:
	static void registerRoutes(Pistache::Rest::Router& router);

private:
	static Pistache::Rest::Route::Result getModules(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response);

	static Pistache::Rest::Route::Result getModuleCount(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response);

	static Pistache::Rest::Route::Result getOtherModules(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response);

	static Pistache::Rest::Route::Result getModuleById(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response);

	static Pistache::Rest::Route::Result getModuleByGuid(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response);

	static Pistache::Rest::Route::Result getEventLog(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response);

	static Pistache::Rest::Route::Result getEventLogCSV(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response);

	static Pistache::Rest::Route::Result getDiagnosticLog(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response);

	static Pistache::Rest::Route::Result getDiagnosticLogCSV(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response);
};