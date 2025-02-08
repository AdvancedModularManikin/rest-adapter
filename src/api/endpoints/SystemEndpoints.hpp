#pragma once

#include <pistache/router.h>
#include <pistache/http.h>
#include <rapidjson/document.h>
#include <thread>

#include "services/SystemService.hpp"
#include "utils/JsonUtils.hpp"
#include "utils/ErrorHandler.hpp"
#include "utils/Exceptions.hpp"
#include "utils/HandlerWrapper.hpp"

class SystemEndpoints {
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

		// System status endpoints
		Routes::Get(router, "/ready",
		            bindWithWrapper(&SystemEndpoints::handleReady));

		Routes::Get(router, "/instance",
		            bindWithWrapper(&SystemEndpoints::getInstance));

		// System control endpoints
		Routes::Get(router, "/debug",
		            bindWithWrapper(&SystemEndpoints::doDebug));

		Routes::Get(router, "/shutdown",
		            bindWithWrapper(&SystemEndpoints::doShutdown));

		// Command endpoints
		Routes::Post(router, "/execute",
		             bindWithWrapper(&SystemEndpoints::executeCommand));

		Routes::Post(router, "/topic/physiology_modification",
		             bindWithWrapper(&SystemEndpoints::executePhysiologyModification));

		Routes::Post(router, "/topic/render_modification",
		             bindWithWrapper(&SystemEndpoints::executeRenderModification));
	}

private:
	static Pistache::Rest::Route::Result handleReady(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response) {
		try {
			addResponseHeaders(response, false);
			response.send(Pistache::Http::Code::Ok,
			              SystemService::getInstance().isReady() ? "1" : "0");
		}
		catch (const std::exception& e) {
			throw ValidationException("Health check failed: " + std::string(e.what()));
		}
		return Pistache::Rest::Route::Result::Ok;
	}

	static Pistache::Rest::Route::Result doDebug(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response) {
		try {
			SystemService::getInstance().setDebugMode(true);
			addResponseHeaders(response, false);
			response.send(Pistache::Http::Code::Ok, "Debug mode enabled");
		}
		catch (const std::exception& e) {
			throw ValidationException("Failed to enable debug mode: " +
			                          std::string(e.what()));
		}
		return Pistache::Rest::Route::Result::Ok;
	}

	static Pistache::Rest::Route::Result getInstance(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response) {
		try {
			auto instance = SystemService::getInstance().getInstanceInfo();
			auto jsonResponse = JsonUtils::serializeInstance(instance);

			addResponseHeaders(response);
			response.send(Pistache::Http::Code::Ok, jsonResponse);
		}
		catch (const std::exception& e) {
			throw ValidationException("Failed to get instance info: " +
			                          std::string(e.what()));
		}
		return Pistache::Rest::Route::Result::Ok;
	}

	static Pistache::Rest::Route::Result doShutdown(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response) {
		try {
			SystemService::getInstance().initiateShutdown();

			addResponseHeaders(response, false);
			response.send(Pistache::Http::Code::Ok, "Shutdown initiated");

			// Give time for the response to be sent before actual shutdown
			std::thread([]{
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				std::exit(0);
			}).detach();
		}
		catch (const std::exception& e) {
			throw ValidationException("Failed to initiate shutdown: " +
			                          std::string(e.what()));
		}
		return Pistache::Rest::Route::Result::Ok;
	}

	static Pistache::Rest::Route::Result executeCommand(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response) {
		try {
			rapidjson::Document document;
			document.Parse(request.body().c_str());

			if (document.HasParseError()) {
				throw ValidationException("Invalid JSON format");
			}

			if (!document.HasMember("payload") || !document["payload"].IsString()) {
				throw ValidationException("Missing or invalid 'payload' field");
			}

			std::string payload = document["payload"].GetString();
			SystemService::getInstance().executeCommand(payload);

			addResponseHeaders(response);
			response.send(Pistache::Http::Code::Ok,
			              R"({"message":"Command executed successfully"})");
		}
		catch (const std::exception& e) {
			throw ValidationException("Failed to execute command: " +
			                          std::string(e.what()));
		}
		return Pistache::Rest::Route::Result::Ok;
	}

	static Pistache::Rest::Route::Result executePhysiologyModification(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response) {
		try {
			auto modData = JsonUtils::parsePhysiologyModification(request.body());
			SystemService::getInstance().executePhysiologyModification(modData);

			addResponseHeaders(response);
			response.send(Pistache::Http::Code::Ok,
			              R"({"message":"Physiology modification published successfully"})");
		}
		catch (const JsonUtils::ParseException& e) {
			throw ValidationException("Invalid request format: " + std::string(e.what()));
		}
		catch (const std::exception& e) {
			throw ValidationException("Failed to execute physiology modification: " +
			                          std::string(e.what()));
		}
		return Pistache::Rest::Route::Result::Ok;
	}

	static Pistache::Rest::Route::Result executeRenderModification(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response) {
		try {
			auto modData = JsonUtils::parseRenderModification(request.body());
			SystemService::getInstance().executeRenderModification(modData);

			addResponseHeaders(response);
			response.send(Pistache::Http::Code::Ok,
			              R"({"message":"Render modification published successfully"})");
		}
		catch (const JsonUtils::ParseException& e) {
			throw ValidationException("Invalid request format: " + std::string(e.what()));
		}
		catch (const std::exception& e) {
			throw ValidationException("Failed to execute render modification: " +
			                          std::string(e.what()));
		}
		return Pistache::Rest::Route::Result::Ok;
	}
};