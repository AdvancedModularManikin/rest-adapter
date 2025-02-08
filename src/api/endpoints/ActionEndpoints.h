#pragma once

#include <pistache/router.h>
#include <pistache/http.h>

#include <rapidjson/document.h>

#include "services/ActionService.h"
#include "utils/JsonUtils.h"
#include "utils/ErrorHandler.h"
#include "utils/Exceptions.h"
#include "utils/HandlerWrapper.h"

class ActionEndpoints {
public:
	static void registerRoutes(Pistache::Rest::Router& router);

private:
	static Pistache::Rest::Route::Result getActions(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response);

	static Pistache::Rest::Route::Result getAction(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response);

	static Pistache::Rest::Route::Result createAction(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response);

	static Pistache::Rest::Route::Result updateAction(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response);

	static Pistache::Rest::Route::Result deleteAction(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response);
};