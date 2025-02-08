#include "Router.h"

void Router::initializeRoutes(Pistache::Http::Endpoint& endpoint) {
	auto opts = Pistache::Http::Endpoint::options()
			.maxRequestSize(1024 * 1024)  // 1MB max request size
			.maxResponseSize(1024 * 1024);  // 1MB max response size

	m_router = std::make_unique<Pistache::Rest::Router>();

	// Set up CORS handlers
	setupCORSHandlers();

	// Register all endpoint groups with error handling wrappers
	registerEndpoints();

	// Set up not found handler
	Pistache::Rest::Routes::NotFound(*m_router,
	                                 Pistache::Rest::Routes::bind(&Router::handleNotFound));

	// Attach router to endpoint
	endpoint.setHandler(m_router->handler());
}

void Router::setupCORSHandlers() {
	// Handle OPTIONS requests for CORS
	Pistache::Rest::Routes::Options(*m_router, "/*",
	                                Pistache::Rest::Routes::bind(&Router::handleCorsOptions));

	// Add CORS headers to all responses using basic routes
	Pistache::Rest::Routes::Get(*m_router, "/*",
	                            Pistache::Rest::Routes::bind(&Router::handleCors));
	Pistache::Rest::Routes::Post(*m_router, "/*",
	                             Pistache::Rest::Routes::bind(&Router::handleCors));
	Pistache::Rest::Routes::Put(*m_router, "/*",
	                            Pistache::Rest::Routes::bind(&Router::handleCors));
	Pistache::Rest::Routes::Delete(*m_router, "/*",
	                               Pistache::Rest::Routes::bind(&Router::handleCors));
}

void Router::registerEndpoints() {
	// Register all endpoint groups
	ActionEndpoints::registerRoutes(*m_router);
	AssessmentEndpoints::registerRoutes(*m_router);
	ModuleEndpoints::registerRoutes(*m_router);
	StatusEndpoints::registerRoutes(*m_router);
	SystemEndpoints::registerRoutes(*m_router);
}

Pistache::Rest::Route::Result Router::handleCorsOptions(
		const Pistache::Rest::Request& request,
		Pistache::Http::ResponseWriter response) {
	response.headers()
			.add<Pistache::Http::Header::AccessControlAllowOrigin>("*")
			.add<Pistache::Http::Header::AccessControlAllowMethods>(
					"GET, POST, PUT, DELETE, OPTIONS")
			.add<Pistache::Http::Header::AccessControlAllowHeaders>(
					"Content-Type")
			.add<Pistache::Http::Header::AccessControlAllowHeaders>(
					"Content-Type, Authorization, X-Requested-With");
	response.send(Pistache::Http::Code::Ok);
	return Pistache::Rest::Route::Result::Ok;
}

Pistache::Rest::Route::Result Router::handleCors(
		const Pistache::Rest::Request& request,
		Pistache::Http::ResponseWriter response) {
	response.headers()
			.add<Pistache::Http::Header::AccessControlAllowOrigin>("*")
			.add<Pistache::Http::Header::AccessControlAllowHeaders>(
					"Content-Type");
	return Pistache::Rest::Route::Result::Ok;
}

Pistache::Rest::Route::Result Router::handleNotFound(
		const Pistache::Rest::Request& request,
		Pistache::Http::ResponseWriter response) {
	response.headers()
			.add<Pistache::Http::Header::AccessControlAllowOrigin>("*")
			.add<Pistache::Http::Header::AccessControlAllowHeaders>(
					"Content-Type");
	response.send(Pistache::Http::Code::Not_Found, "Route not found");
	return Pistache::Rest::Route::Result::Ok;
}