#include "ModuleEndpoints.h"


void ModuleEndpoints::registerRoutes(Pistache::Rest::Router& router) {
	using namespace Pistache::Rest;
	using namespace Pistache::Http;

	Routes::Get(router, "/modules",
	            HandlerWrapper::WrapHandler(&ModuleEndpoints::getModules));

	Routes::Get(router, "/modules/count",
	            HandlerWrapper::WrapHandler(&ModuleEndpoints::getModuleCount));

	Routes::Get(router, "/modules/other",
	            HandlerWrapper::WrapHandler(&ModuleEndpoints::getOtherModules));

	Routes::Get(router, "/module/id/:id",
	            HandlerWrapper::WrapHandler(&ModuleEndpoints::getModuleById));

	Routes::Get(router, "/module/guid/:guid",
	            HandlerWrapper::WrapHandler(&ModuleEndpoints::getModuleByGuid));

	Routes::Get(router, "/events",
	            HandlerWrapper::WrapHandler(&ModuleEndpoints::getEventLog));

	Routes::Get(router, "/events/csv",
	            HandlerWrapper::WrapHandler(&ModuleEndpoints::getEventLogCSV));

	Routes::Get(router, "/logs",
	            HandlerWrapper::WrapHandler(&ModuleEndpoints::getDiagnosticLog));

	Routes::Get(router, "/logs/csv",
	            HandlerWrapper::WrapHandler(&ModuleEndpoints::getDiagnosticLogCSV));
}

Pistache::Rest::Route::Result ModuleEndpoints::getModules(
		const Pistache::Rest::Request& request,
		Pistache::Http::ResponseWriter response) {
	auto modules = ModuleService::getInstance().getAllModules();
	auto jsonResponse = JsonUtils::serializeModules(modules);
	response.headers()
			.add<Pistache::Http::Header::ContentType>(MIME(Application, Json));
	response.send(Pistache::Http::Code::Ok, jsonResponse);
	return Pistache::Rest::Route::Result::Ok;
}

Pistache::Rest::Route::Result ModuleEndpoints::getModuleCount(
		const Pistache::Rest::Request& request,
		Pistache::Http::ResponseWriter response) {
	auto counts = ModuleService::getInstance().getModuleCounts();
	auto jsonResponse = JsonUtils::serializeModuleCounts(counts);
	response.headers()
			.add<Pistache::Http::Header::ContentType>(MIME(Application, Json));
	response.send(Pistache::Http::Code::Ok, jsonResponse);
	return Pistache::Rest::Route::Result::Ok;
}

Pistache::Rest::Route::Result ModuleEndpoints::getOtherModules(
		const Pistache::Rest::Request& request,
		Pistache::Http::ResponseWriter response) {
	auto modules = ModuleService::getInstance().getOtherModules();
	auto jsonResponse = JsonUtils::serializeModuleNames(modules);
	response.headers()
			.add<Pistache::Http::Header::ContentType>(MIME(Application, Json));
	response.send(Pistache::Http::Code::Ok, jsonResponse);
	return Pistache::Rest::Route::Result::Ok;
}

Pistache::Rest::Route::Result ModuleEndpoints::getModuleById(
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

Pistache::Rest::Route::Result ModuleEndpoints::getModuleByGuid(
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

Pistache::Rest::Route::Result ModuleEndpoints::getEventLog(
		const Pistache::Rest::Request& request,
		Pistache::Http::ResponseWriter response) {
	auto events = ModuleService::getInstance().getEventLog();
	auto jsonResponse = JsonUtils::serializeEventLog(events);
	response.headers()
			.add<Pistache::Http::Header::ContentType>(MIME(Application, Json));
	response.send(Pistache::Http::Code::Ok, jsonResponse);
	return Pistache::Rest::Route::Result::Ok;
}

Pistache::Rest::Route::Result ModuleEndpoints::getEventLogCSV(
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

Pistache::Rest::Route::Result ModuleEndpoints::getDiagnosticLog(
		const Pistache::Rest::Request& request,
		Pistache::Http::ResponseWriter response) {
	auto logs = ModuleService::getInstance().getDiagnosticLog();
	auto jsonResponse = JsonUtils::serializeDiagnosticLog(logs);
	response.headers()
			.add<Pistache::Http::Header::ContentType>(MIME(Application, Json));
	response.send(Pistache::Http::Code::Ok, jsonResponse);
	return Pistache::Rest::Route::Result::Ok;
}

Pistache::Rest::Route::Result ModuleEndpoints::getDiagnosticLogCSV(
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