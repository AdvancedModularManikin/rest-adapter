#pragma once

#include <pistache/router.h>
#include <pistache/http.h>
#include <rapidjson/document.h>

#include "services/ModuleService.hpp"
#include "utils/JsonUtils.hpp"
#include "utils/ErrorHandler.hpp"
#include "utils/Exceptions.hpp"
#include "utils/HandlerWrapper.hpp"

class ModuleEndpoints {
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

		Routes::Get(router, "/modules",
		            bindWithWrapper(&ModuleEndpoints::getModules));

		Routes::Get(router, "/modules/count",
		            bindWithWrapper(&ModuleEndpoints::getModuleCount));

		Routes::Get(router, "/modules/other",
		            bindWithWrapper(&ModuleEndpoints::getOtherModules));

		Routes::Get(router, "/module/id/:id",
		            bindWithWrapper(&ModuleEndpoints::getModuleById));

		Routes::Get(router, "/module/guid/:guid",
		            bindWithWrapper(&ModuleEndpoints::getModuleByGuid));

		Routes::Get(router, "/events",
		            bindWithWrapper(&ModuleEndpoints::getEventLog));

		Routes::Get(router, "/events/csv",
		            bindWithWrapper(&ModuleEndpoints::getEventLogCSV));

		Routes::Get(router, "/logs",
		            bindWithWrapper(&ModuleEndpoints::getDiagnosticLog));

		Routes::Get(router, "/logs/csv",
		            bindWithWrapper(&ModuleEndpoints::getDiagnosticLogCSV));
	}

private:
	static Pistache::Rest::Route::Result getModules(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response) {
		auto modules = ModuleService::getInstance().getAllModules();
		auto jsonResponse = JsonUtils::serializeModules(modules);
		response.headers()
				.add<Pistache::Http::Header::ContentType>(MIME(Application, Json));
		response.send(Pistache::Http::Code::Ok, jsonResponse);
		return Pistache::Rest::Route::Result::Ok;
	}

	static Pistache::Rest::Route::Result getModuleCount(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response) {
		auto counts = ModuleService::getInstance().getModuleCounts();
		auto jsonResponse = JsonUtils::serializeModuleCounts(counts);
		response.headers()
				.add<Pistache::Http::Header::ContentType>(MIME(Application, Json));
		response.send(Pistache::Http::Code::Ok, jsonResponse);
		return Pistache::Rest::Route::Result::Ok;
	}

	static Pistache::Rest::Route::Result getOtherModules(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response) {
		auto modules = ModuleService::getInstance().getOtherModules();
		auto jsonResponse = JsonUtils::serializeModuleNames(modules);
		response.headers()
				.add<Pistache::Http::Header::ContentType>(MIME(Application, Json));
		response.send(Pistache::Http::Code::Ok, jsonResponse);
		return Pistache::Rest::Route::Result::Ok;
	}

	static Pistache::Rest::Route::Result getModuleById(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response) {
		auto id = request.param(":id").as<std::string>();

		auto module = ModuleService::getInstance().getModuleById(id);
		if (!module) {
			throw ResourceNotFoundException("Module not found with ID: " + id);
		}

		auto jsonResponse = JsonUtils::serializeModule(*module);
		response.headers()
				.add<Pistache::Http::Header::ContentType>(MIME(Application, Json));
		response.send(Pistache::Http::Code::Ok, jsonResponse);
		return Pistache::Rest::Route::Result::Ok;
	}

	static Pistache::Rest::Route::Result getModuleByGuid(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response) {
		auto guid = request.param(":guid").as<std::string>();

		auto module = ModuleService::getInstance().getModuleByGuid(guid);
		if (!module) {
			throw ResourceNotFoundException("Module not found with GUID: " + guid);
		}

		auto jsonResponse = JsonUtils::serializeModule(*module);
		response.headers()
				.add<Pistache::Http::Header::ContentType>(MIME(Application, Json));
		response.send(Pistache::Http::Code::Ok, jsonResponse);
		return Pistache::Rest::Route::Result::Ok;
	}

	static Pistache::Rest::Route::Result getEventLog(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response) {
		auto events = ModuleService::getInstance().getEventLog();
		auto jsonResponse = JsonUtils::serializeEventLog(events);
		response.headers()
				.add<Pistache::Http::Header::ContentType>(MIME(Application, Json));
		response.send(Pistache::Http::Code::Ok, jsonResponse);
		return Pistache::Rest::Route::Result::Ok;
	}

	static Pistache::Rest::Route::Result getEventLogCSV(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response) {
		auto events = ModuleService::getInstance().getEventLog();
		auto csvContent = JsonUtils::createEventLogCSV(events);
		auto mime = Pistache::Http::Mime::MediaType::fromString("text/csv");
		response.headers()
				.add<Pistache::Http::Header::ContentType>(mime);
				//.add<Pistache::Http::Header::ContentDisposition>(
						//"attachment; filename=\"event_log.csv\"");
		response.send(Pistache::Http::Code::Ok, csvContent);
		return Pistache::Rest::Route::Result::Ok;
	}

	static Pistache::Rest::Route::Result getDiagnosticLog(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response) {
		auto logs = ModuleService::getInstance().getDiagnosticLog();
		auto jsonResponse = JsonUtils::serializeDiagnosticLog(logs);
		response.headers()
				.add<Pistache::Http::Header::ContentType>(MIME(Application, Json));
		response.send(Pistache::Http::Code::Ok, jsonResponse);
		return Pistache::Rest::Route::Result::Ok;
	}

	static Pistache::Rest::Route::Result getDiagnosticLogCSV(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response) {
		auto logs = ModuleService::getInstance().getDiagnosticLog();
		auto csvContent = JsonUtils::createDiagnosticLogCSV(logs);
		auto mime = Pistache::Http::Mime::MediaType::fromString("text/csv");
		response.headers()
				.add<Pistache::Http::Header::ContentType>(mime);
				//.add<Pistache::Http::Header::ContentDisposition>(
						//"attachment; filename=\"diagnostic_log.csv\"");
		response.send(Pistache::Http::Code::Ok, csvContent);
		return Pistache::Rest::Route::Result::Ok;
	}
};