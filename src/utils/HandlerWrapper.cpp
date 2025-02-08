#include "HandlerWrapper.h"


void HandlerWrapper::addCorsHeaders(Pistache::Http::ResponseWriter& response) {
	response.headers()
			.add<Pistache::Http::Header::AccessControlAllowOrigin>("*")
			.add<Pistache::Http::Header::AccessControlAllowHeaders>("Content-Type");
}