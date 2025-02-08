#pragma once

#include <pistache/router.h>
#include <pistache/http.h>
#include <rapidjson/document.h>

#include "services/ActionService.hpp"
#include "utils/JsonUtils.hpp"
#include "utils/ErrorHandler.hpp"
#include "utils/Exceptions.hpp"
#include "utils/HandlerWrapper.hpp"

class ActionEndpoints {
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
		Pistache::Rest::Routes::Get(router, "/actions",
		                            bindWithWrapper(&ActionEndpoints::getActions));

		Pistache::Rest::Routes::Get(router, "/action/:name",
		                            bindWithWrapper(&ActionEndpoints::getAction));

		Pistache::Rest::Routes::Post(router, "/action",
		                             bindWithWrapper(&ActionEndpoints::createAction));

		Pistache::Rest::Routes::Put(router, "/action/:name",
		                            bindWithWrapper(&ActionEndpoints::updateAction));

		Pistache::Rest::Routes::Delete(router, "/action/:name",
		                               bindWithWrapper(&ActionEndpoints::deleteAction));
	}

private:
	static Pistache::Rest::Route::Result getActions(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response) {
		auto actions = ActionService::getInstance().getAllActions();
		auto jsonResponse = JsonUtils::serializeActions(actions);

		response.headers()
				.add<Pistache::Http::Header::ContentType>(MIME(Application, Json));
		response.send(Pistache::Http::Code::Ok, jsonResponse);
		return Pistache::Rest::Route::Result::Ok;
	}

	static Pistache::Rest::Route::Result getAction(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response) {
		auto name = request.param(":name").as<std::string>();

		auto action = ActionService::getInstance().getAction(name);
		if (!action) {
			throw ResourceNotFoundException("Action not found: " + name);
		}

		auto jsonResponse = JsonUtils::serializeAction(*action);

		response.headers()
				.add<Pistache::Http::Header::ContentType>(MIME(Application, Json));
		response.send(Pistache::Http::Code::Ok, jsonResponse);
		return Pistache::Rest::Route::Result::Ok;
	}

	static Pistache::Rest::Route::Result createAction(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response) {
		try {
			auto action = JsonUtils::parseAction(request.body());
			ActionService::getInstance().createAction(action);

			response.headers()
					.add<Pistache::Http::Header::ContentType>(MIME(Application, Json));
			response.send(Pistache::Http::Code::Created,
			              R"({"message":"Action created successfully"})");
		}
		catch (const JsonUtils::ParseException& e) {
			throw ValidationException("Invalid request format: " +
			                          std::string(e.what()));
		}
		return Pistache::Rest::Route::Result::Ok;
	}

	static Pistache::Rest::Route::Result updateAction(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response) {
		auto name = request.param(":name").as<std::string>();

		try {
			auto action = JsonUtils::parseAction(request.body());
			if (action.name != name) {
				throw ValidationException("Action name mismatch");
			}

			ActionService::getInstance().updateAction(action);

			response.headers()
					.add<Pistache::Http::Header::ContentType>(MIME(Application, Json));
			response.send(Pistache::Http::Code::Ok,
			              R"({"message":"Action updated successfully"})");
		}
		catch (const JsonUtils::ParseException& e) {
			throw ValidationException("Invalid request format: " +
			                          std::string(e.what()));
		}
		return Pistache::Rest::Route::Result::Ok;
	}

	static Pistache::Rest::Route::Result deleteAction(
			const Pistache::Rest::Request& request,
			Pistache::Http::ResponseWriter response) {
		auto name = request.param(":name").as<std::string>();

		if (!ActionService::getInstance().deleteAction(name)) {
			throw ResourceNotFoundException("Action not found: " + name);
		}

		response.headers()
				.add<Pistache::Http::Header::ContentType>(MIME(Application, Json));
		response.send(Pistache::Http::Code::Ok,
		              R"({"message":"Action deleted successfully"})");
		return Pistache::Rest::Route::Result::Ok;
	}
};