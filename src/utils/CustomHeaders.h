#pragma once

#include <string>
#include <pistache/http.h>

namespace Pistache {
	namespace Http {
		namespace Header {

			class XFrameOptions : public Header {
			public:
				NAME("X-Frame-Options")
				explicit XFrameOptions(const std::string& value);
				void write(std::ostream& os) const;

			private:
				std::string value_;
			};

			class XContentTypeOptions : public Header {
			public:
				NAME("X-Content-Type-Options")
				explicit XContentTypeOptions(const std::string& value);
				void write(std::ostream& os) const;

			private:
				std::string value_;
			};

			class XXSSProtection : public Header {
			public:
				NAME("X-XSS-Protection")
				explicit XXSSProtection(const std::string& value);
				void write(std::ostream& os) const;

			private:
				std::string value_;
			};

		} // namespace Header
	} // namespace Http
} // namespace Pistache