#include "CustomHeaders.h"

namespace Pistache {
	namespace Http {
		namespace Header {

			XFrameOptions::XFrameOptions(const std::string& value) : value_(value) {}

			void XFrameOptions::write(std::ostream& os) const {
				os << value_;
			}

			XContentTypeOptions::XContentTypeOptions(const std::string& value) : value_(value) {}

			void XContentTypeOptions::write(std::ostream& os) const {
				os << value_;
			}

			XXSSProtection::XXSSProtection(const std::string& value) : value_(value) {}

			void XXSSProtection::write(std::ostream& os) const {
				os << value_;
			}

		} // namespace Header
	} // namespace Http
} // namespace Pistache