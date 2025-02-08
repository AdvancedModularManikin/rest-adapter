#include "ActionEndpoints.h"


void ActionEndpoints::registerRoutes(Pistache::Rest::Router& router) {
	Pistache::Rest::Routes::Get(router, "/actions",
	                            HandlerWrapper::WrapHandler(&ActionEndpoints::getActions));

	Pistache::Rest::Routes::Get(router, "/action/:name",
	                            HandlerWrapper::WrapHandler(&ActionEndpoints::getAction));

	Pistache::Rest::Routes::Post(router, "/action",
	                             HandlerWrapper::WrapHandler(&ActionEndpoints::createAction));

	Pistache::Rest::Routes::Put(router, "/action/:name",
	                            HandlerWrapper::WrapHandler(&ActionEndpoints::updateAction));

	Pistache::Rest::Routes::Delete(router, "/action/:name",
	                               HandlerWrapper::WrapHandler(&ActionEndpoints::deleteAction));
}

Pistache::Rest::Route::Result ActionEndpoints::getActions(
		const Pistache::Rest::Request& request,
		Pistache::Http::ResponseWriter response) {
	auto actions = ActionService::getInstance().getAllActions();
	auto jsonResponse = JsonUtils::serializeActions(actions);

	response.headers()
			.add<Pistache::Http::Header::ContentType>(MIME(Application, Json));
	response.send(Pistache::Http::Code::Ok, jsonResponse);
	return Pistache::Rest::Route::Result::Ok;
}

Pistache::Rest::Route::Result ActionEndpoints::getAction(
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

Pistache::Rest::Route::Result ActionEndpoints::createAction(
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

Pistache::Rest::Route::Result ActionEndpoints::updateAction(
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

Pistache::Rest::Route::Result ActionEndpoints::deleteAction(
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