#pragma once

#include <string>

namespace Queries {
	// Module queries
	static const char* GET_MODULES = R"(
        SELECT
            module_id,
            module_name,
            description,
            capabilities,
            manufacturer,
            model
        FROM module_capabilities
    )";

	static const char* GET_MODULE_BY_ID = R"(
        SELECT
            module_id,
            module_name,
            description,
            capabilities,
            manufacturer,
            model
        FROM module_capabilities
        WHERE module_id = ?
    )";

	static const char* GET_MODULE_BY_GUID = R"(
        SELECT
            module_id,
            module_guid,
            module_name,
            capabilities,
            manufacturer,
            model
        FROM module_capabilities
        WHERE module_guid = ?
    )";

	static const char* GET_MODULE_COUNT = R"(
        SELECT
            COUNT(DISTINCT module_name) as total,
            SUM(CASE WHEN module_name LIKE 'AMM_%' THEN 1 ELSE 0 END) as core
        FROM module_capabilities
    )";

	static const char* GET_OTHER_MODULES = R"(
        SELECT DISTINCT module_name
        FROM module_capabilities
        WHERE module_name NOT LIKE 'AMM_%'
    )";

	// Event log queries
	static const char* GET_EVENT_LOG = R"(
        SELECT
            mc.module_id,
            mc.module_name,
            e.source,
            e.topic,
            e.timestamp,
            e.data
        FROM events e
        LEFT JOIN module_capabilities mc ON e.source = mc.module_guid
        ORDER BY e.timestamp DESC
    )";

	// Diagnostic log queries
	static const char* GET_DIAGNOSTIC_LOG = R"(
        SELECT
            module_name,
            module_guid,
            module_id,
            message,
            log_level,
            timestamp
        FROM logs
        ORDER BY timestamp DESC
    )";
}
