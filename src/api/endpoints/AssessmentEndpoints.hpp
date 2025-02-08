#pragma once

#include <pistache/router.h>
#include <pistache/http.h>
#include <rapidjson/document.h>

#include "core/MoHSESManager.hpp"

#include "services/AssessmentService.hpp"

#include "utils/JsonUtils.hpp"
#include "utils/ErrorHandler.hpp"
#include "utils/Exceptions.hpp"
#include "utils/HandlerWrapper.hpp"

class AssessmentEndpoints {
private:
	template<typename Handler>
	static Pistache::Rest::Route::Handler bindWithWrapper(Handler handler) {
		return Pistache::Rest::Route::Handler(
				[handler](const Pistache::Rest::Request& req,
				          Pistache::Http::ResponseWriter resp) -> Pistache::Rest::Route::Result {
					return HandlerWrapper::WrapHandler(handler)(req, std::move(resp));
				});
	}

public:
	static void registerRoutes(Pistache::Rest::Router& router) {
		using namespace Pistache::Rest;
		using namespace Pistache::Http;

		Routes::Get(router, "/assessments",
		            bindWithWrapper(&AssessmentEndpoints::getAssessments));

		Routes::Get(router, "/assessment/:name",
		            bindWithWrapper(&AssessmentEndpoints::getAssessment));

		Routes::Post(router, "/assessment/:name",
		             bindWithWrapper(&AssessmentEndpoints::createAssessment));

		Routes::Put(router, "/assessment/:name",
		            bindWithWrapper(&AssessmentEndpoints::updateAssessment));

		Routes::Delete(router, "/assessment/:name",
		               bindWithWrapper(&AssessmentEndpoints::deleteAssessment));

		Routes::Post(router, "/topic/performance_assessment",
		             bindWithWrapper(&AssessmentEndpoints::executePerformanceAssessment));
	}

private:
	static Pistache::Rest::Route::Result getAssessments(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response) {
		auto assessments = AssessmentService::getInstance().getAllAssessments();
		auto jsonResponse = JsonUtils::serializeAssessments(assessments);

		response.headers()
				.add<Pistache::Http::Header::ContentType>(MIME(Application, Json));
		response.send(Pistache::Http::Code::Ok, jsonResponse);
		return Pistache::Rest::Route::Result::Ok;
	}

	static Pistache::Rest::Route::Result getAssessment(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response) {
		auto name = request.param(":name").as<std::string>();

		auto assessment = AssessmentService::getInstance().getAssessment(name);
		if (!assessment) {
			throw ResourceNotFoundException("Assessment not found: " + name);
		}

		// Serve the file directly with appropriate headers
		// response.headers()
				//.add<Pistache::Http::Header::ContentType>(MIME(Text, Csv))
				// .add<Pistache::Http::Header::ContentDisposition>("attachment; filename=\"" + name + "\"");
		response.send(Pistache::Http::Code::Ok, assessment->content);
		return Pistache::Rest::Route::Result::Ok;
	}

	static Pistache::Rest::Route::Result createAssessment(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response) {
		auto name = request.param(":name").as<std::string>();

		try {
			Assessment assessment;
			assessment.name = name;
			assessment.content = request.body();

			AssessmentService::getInstance().createAssessment(assessment);

			MoHSESManager::getInstance().sendCommand("[SYS]ASSESSMENT_AVAILABLE:" + name);
			response.headers()
					.add<Pistache::Http::Header::ContentType>(MIME(Application, Json));
			response.send(Pistache::Http::Code::Created,
			              R"({"message":"Assessment created successfully"})");
		}
		catch (const std::exception& e) {
			throw ValidationException("Failed to create assessment: " +
			                          std::string(e.what()));
		}
		return Pistache::Rest::Route::Result::Ok;
	}

	static Pistache::Rest::Route::Result updateAssessment(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response) {
		auto name = request.param(":name").as<std::string>();

		try {
			Assessment assessment;
			assessment.name = name;
			assessment.content = request.body();

			AssessmentService::getInstance().updateAssessment(assessment);
			response.headers()
					.add<Pistache::Http::Header::ContentType>(MIME(Application, Json));
			response.send(Pistache::Http::Code::Ok,
			              R"({"message":"Assessment updated successfully"})");
		}
		catch (const std::exception& e) {
			throw ValidationException("Failed to update assessment: " +
			                          std::string(e.what()));
		}
		return Pistache::Rest::Route::Result::Ok;
	}

	static Pistache::Rest::Route::Result deleteAssessment(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response) {
		auto name = request.param(":name").as<std::string>();

		if (!AssessmentService::getInstance().deleteAssessment(name)) {
			throw ResourceNotFoundException("Assessment not found: " + name);
		}
		response.headers()
				.add<Pistache::Http::Header::ContentType>(MIME(Application, Json));
		response.send(Pistache::Http::Code::Ok,
		              R"({"message":"Assessment deleted successfully"})");
		return Pistache::Rest::Route::Result::Ok;
	}

	static Pistache::Rest::Route::Result executePerformanceAssessment(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response) {
		try {
			auto assessmentData = JsonUtils::parsePerformanceAssessment(request.body());

			// Create event record and get UUID
			auto eventUUID = MoHSESManager::getInstance().sendEventRecord(
					assessmentData.location,
					assessmentData.practitioner,
					assessmentData.type
			);

			// Send performance assessment
			AssessmentService::getInstance().sendPerformanceAssessment(
					eventUUID,
					assessmentData
			);
			response.headers()
					.add<Pistache::Http::Header::ContentType>(MIME(Application, Json));
			response.send(Pistache::Http::Code::Ok,
			              R"({"message":"Performance assessment published"})");
		}
		catch (const JsonUtils::ParseException& e) {
			throw ValidationException("Invalid request format: " +
			                          std::string(e.what()));
		}
		return Pistache::Rest::Route::Result::Ok;
	}
};