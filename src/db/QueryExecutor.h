#pragma once

#include <optional>
#include <vector>

#include "DatabaseConnection.h"
#include "core/Types.h"

#include "utils/Exceptions.h"
#include "Queries.h"

#include "amm/BaseLogger.h"

class QueryExecutor {
public:
	static QueryExecutor& getInstance();

	std::vector<Module> getModules();
	std::optional<Module> getModuleById(const std::string& id);
	std::optional<Module> getModuleByGuid(const std::string& guid);
	ModuleCounts getModuleCounts();
	std::vector<std::string> getOtherModules();
	std::vector<EventLogEntry> getEventLog();
	std::vector<DiagnosticLogEntry> getDiagnosticLog();

private:
	QueryExecutor();
	QueryExecutor(const QueryExecutor&) = delete;
	QueryExecutor& operator=(const QueryExecutor&) = delete;

	DatabaseConnection m_db;
};