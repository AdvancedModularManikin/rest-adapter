#pragma once

#include <string>
#include <vector>
#include <optional>
#include <mutex>
#include "core/Types.h"
#include "core/Config.h"
#include "db/QueryExecutor.h"
#include "utils/Exceptions.h"
#include "amm/BaseLogger.h"

class ModuleService {
public:
	static ModuleService& getInstance();

	std::vector<Module> getAllModules();
	ModuleCounts getModuleCounts();
	std::vector<std::string> getOtherModules();
	std::optional<Module> getModuleById(const std::string& id);
	std::optional<Module> getModuleByGuid(const std::string& guid);
	std::vector<EventLogEntry> getEventLog();
	std::vector<DiagnosticLogEntry> getDiagnosticLog();

private:
	ModuleService() = default;
	ModuleService(const ModuleService&) = delete;
	ModuleService& operator=(const ModuleService&) = delete;

	std::mutex m_mutex;
};