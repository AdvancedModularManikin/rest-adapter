#pragma once

#include <pistache/router.h>
#include <pistache/http.h>

#include <rapidjson/document.h>

#include "core/MoHSESManager.h"
#include "services/AssessmentService.h"
#include "utils/JsonUtils.h"
#include "utils/ErrorHandler.h"
#include "utils/Exceptions.h"
#include "utils/HandlerWrapper.h"

class AssessmentEndpoints {
public:
	static void registerRoutes(Pistache::Rest::Router& router);

private:
	static Pistache::Rest::Route::Result getAssessments(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response);

	static Pistache::Rest::Route::Result getAssessment(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response);

	static Pistache::Rest::Route::Result createAssessment(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response);

	static Pistache::Rest::Route::Result updateAssessment(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response);

	static Pistache::Rest::Route::Result deleteAssessment(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response);

	static Pistache::Rest::Route::Result executePerformanceAssessment(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response);
};