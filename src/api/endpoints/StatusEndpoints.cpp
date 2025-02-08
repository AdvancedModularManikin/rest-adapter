#include "StatusEndpoints.h"

void StatusEndpoints::addResponseHeaders(Pistache::Http::ResponseWriter& response,
                                         bool isJson) {
	if (isJson) {
		response.headers()
				.add<Pistache::Http::Header::ContentType>(MIME(Application, Json));
	}
	response.headers()
			.add<Pistache::Http::Header::AccessControlAllowOrigin>("*")
			.add<Pistache::Http::Header::AccessControlAllowHeaders>("Content-Type");
}

void StatusEndpoints::registerRoutes(Pistache::Rest::Router& router) {
	using namespace Pistache::Rest;
	using namespace Pistache::Http;

	Routes::Get(router, "/status",
	            HandlerWrapper::WrapHandler(&StatusEndpoints::getStatus));

	Routes::Get(router, "/status/:key",
	            HandlerWrapper::WrapHandler(&StatusEndpoints::getStatusValue));

	Routes::Get(router, "/nodes",
	            HandlerWrapper::WrapHandler(&StatusEndpoints::getNodes));

	Routes::Get(router, "/node/:name",
	            HandlerWrapper::WrapHandler(&StatusEndpoints::getNode));

	Routes::Get(router, "/labs",
	            HandlerWrapper::WrapHandler(&StatusEndpoints::getLabsReport));
}

Pistache::Rest::Route::Result StatusEndpoints::getStatus(
		const Pistache::Rest::Request& request,
		Pistache::Http::ResponseWriter response) {
	try {
		auto status = StatusService::getInstance().getAllStatus();
		auto jsonResponse = JsonUtils::serializeStatus(status);

		addResponseHeaders(response);
		response.send(Pistache::Http::Code::Ok, jsonResponse);
	}
	catch (const std::exception& e) {
		throw ValidationException("Failed to retrieve status: " +
		                          std::string(e.what()));
	}
	return Pistache::Rest::Route::Result::Ok;
}

Pistache::Rest::Route::Result StatusEndpoints::getStatusValue(
		const Pistache::Rest::Request& request,
		Pistache::Http::ResponseWriter response) {
	auto key = request.param(":key").as<std::string>();

	try {
		auto value = StatusService::getInstance().getStatusValue(key);
		if (!value) {
			throw ResourceNotFoundException("Status key not found: " + key);
		}

		auto jsonResponse = JsonUtils::serializeKeyValue(key, *value);

		addResponseHeaders(response);
		response.send(Pistache::Http::Code::Ok, jsonResponse);
	}
	catch (const ResourceNotFoundException& e) {
		throw;  // Re-throw ResourceNotFoundException
	}
	catch (const std::exception& e) {
		throw ValidationException("Failed to retrieve status value for key '" +
		                          key + "': " + std::string(e.what()));
	}
	return Pistache::Rest::Route::Result::Ok;
}

Pistache::Rest::Route::Result StatusEndpoints::getNodes(
		const Pistache::Rest::Request& request,
		Pistache::Http::ResponseWriter response) {
	try {
		auto nodes = StatusService::getInstance().getAllNodes();
		auto status = StatusService::getInstance().getAllStatus();
		auto jsonResponse = JsonUtils::serializeNodesAndStatus(nodes, status);

		addResponseHeaders(response);
		response.send(Pistache::Http::Code::Ok, jsonResponse);
	}
	catch (const std::exception& e) {
		throw ValidationException("Failed to retrieve nodes: " +
		                          std::string(e.what()));
	}
	return Pistache::Rest::Route::Result::Ok;
}

Pistache::Rest::Route::Result StatusEndpoints::getNode(
		const Pistache::Rest::Request& request,
		Pistache::Http::ResponseWriter response) {
	auto name = request.param(":name").as<std::string>();

	try {
		auto value = StatusService::getInstance().getNodeValue(name);
		if (!value) {
			throw ResourceNotFoundException("Node not found: " + name);
		}

		auto jsonResponse = JsonUtils::serializeKeyValue(name, *value);

		addResponseHeaders(response);
		response.send(Pistache::Http::Code::Ok, jsonResponse);
	}
	catch (const ResourceNotFoundException& e) {
		throw;  // Re-throw ResourceNotFoundException
	}
	catch (const std::exception& e) {
		throw ValidationException("Failed to retrieve node '" + name +
		                          "': " + std::string(e.what()));
	}
	return Pistache::Rest::Route::Result::Ok;
}

Pistache::Rest::Route::Result StatusEndpoints::getLabsReport(
		const Pistache::Rest::Request& request,
		Pistache::Http::ResponseWriter response) {
	try {
		auto labs = StatusService::getInstance().getLabsReport();

		response.headers()
						//.add<Pistache::Http::Header::ContentType>(MIME(Text, Csv))
						//.add<Pistache::Http::Header::ContentDisposition>(
						// "attachment; filename=\"labs_report.csv\"")
				.add<Pistache::Http::Header::AccessControlAllowOrigin>("*")
				.add<Pistache::Http::Header::AccessControlAllowHeaders>("Content-Type");

		response.send(Pistache::Http::Code::Ok, labs);
	}
	catch (const std::exception& e) {
		throw ValidationException("Failed to retrieve labs report: " +
		                          std::string(e.what()));
	}
	return Pistache::Rest::Route::Result::Ok;
}