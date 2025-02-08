#pragma once

#include <pistache/router.h>
#include <pistache/http.h>
#include <rapidjson/document.h>

#include "services/StatusService.hpp"

#include "utils/JsonUtils.hpp"
#include "utils/ErrorHandler.hpp"
#include "utils/Exceptions.hpp"

#include "utils/HandlerWrapper.hpp"

class StatusEndpoints {
private:
	template<typename Handler>
	static Pistache::Rest::Route::Handler bindWithWrapper(Handler handler) {
		return Pistache::Rest::Route::Handler(
				[handler](const Pistache::Rest::Request& req,
				          Pistache::Http::ResponseWriter resp) -> Pistache::Rest::Route::Result {
					return HandlerWrapper::WrapHandler(handler)(req, std::move(resp));
				});
	}

	static void addResponseHeaders(Pistache::Http::ResponseWriter& response,
	                               bool isJson = true) {
		if (isJson) {
			response.headers()
					.add<Pistache::Http::Header::ContentType>(MIME(Application, Json));
		}
		response.headers()
				.add<Pistache::Http::Header::AccessControlAllowOrigin>("*")
				.add<Pistache::Http::Header::AccessControlAllowHeaders>("Content-Type");
	}

public:
	static void registerRoutes(Pistache::Rest::Router& router) {
		using namespace Pistache::Rest;
		using namespace Pistache::Http;

		Routes::Get(router, "/status",
		            bindWithWrapper(&StatusEndpoints::getStatus));

		Routes::Get(router, "/status/:key",
		            bindWithWrapper(&StatusEndpoints::getStatusValue));

		Routes::Get(router, "/nodes",
		            bindWithWrapper(&StatusEndpoints::getNodes));

		Routes::Get(router, "/node/:name",
		            bindWithWrapper(&StatusEndpoints::getNode));

		Routes::Get(router, "/labs",
		            bindWithWrapper(&StatusEndpoints::getLabsReport));
	}

private:
	static Pistache::Rest::Route::Result getStatus(
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

	static Pistache::Rest::Route::Result getStatusValue(
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

	static Pistache::Rest::Route::Result getNodes(
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

	static Pistache::Rest::Route::Result getNode(
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

	static Pistache::Rest::Route::Result getLabsReport(
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
};