#pragma once

#include <string>
#include <vector>
#include <optional>
#include <mutex>

#include "core/Config.hpp"
#include "core/Types.hpp"
#include "db/QueryExecutor.hpp"
#include "utils/Exceptions.hpp"

class ModuleService {
public:
	static ModuleService& getInstance() {
		static ModuleService instance;
		return instance;
	}

	std::vector<Module> getAllModules() {
		std::lock_guard<std::mutex> lock(m_mutex);
		try {
			return QueryExecutor::getInstance().getModules();
		}
		catch (const DatabaseException& e) {
			LOG_ERROR << "Failed to get all modules: " << e.what();
			throw;
		}
	}

	ModuleCounts getModuleCounts() {
		std::lock_guard<std::mutex> lock(m_mutex);
		try {
			return QueryExecutor::getInstance().getModuleCounts();
		}
		catch (const DatabaseException& e) {
			LOG_ERROR << "Failed to get module counts: " << e.what();
			throw;
		}
	}

	std::vector<std::string> getOtherModules() {
		std::lock_guard<std::mutex> lock(m_mutex);
		try {
			return QueryExecutor::getInstance().getOtherModules();
		}
		catch (const DatabaseException& e) {
			LOG_ERROR << "Failed to get other modules: " << e.what();
			throw;
		}
	}

	std::optional<Module> getModuleById(const std::string& id) {
		std::lock_guard<std::mutex> lock(m_mutex);
		try {
			return QueryExecutor::getInstance().getModuleById(id);
		}
		catch (const DatabaseException& e) {
			LOG_ERROR << "Failed to get module by ID " << id << ": " << e.what();
			throw;
		}
	}

	std::optional<Module> getModuleByGuid(const std::string& guid) {
		std::lock_guard<std::mutex> lock(m_mutex);
		try {
			return QueryExecutor::getInstance().getModuleByGuid(guid);
		}
		catch (const DatabaseException& e) {
			LOG_ERROR << "Failed to get module by GUID " << guid << ": " << e.what();
			throw;
		}
	}

	std::vector<EventLogEntry> getEventLog() {
		std::lock_guard<std::mutex> lock(m_mutex);
		try {
			return QueryExecutor::getInstance().getEventLog();
		}
		catch (const DatabaseException& e) {
			LOG_ERROR << "Failed to get event log: " << e.what();
			throw;
		}
	}

	std::vector<DiagnosticLogEntry> getDiagnosticLog() {
		std::lock_guard<std::mutex> lock(m_mutex);
		try {
			return QueryExecutor::getInstance().getDiagnosticLog();
		}
		catch (const DatabaseException& e) {
			LOG_ERROR << "Failed to get diagnostic log: " << e.what();
			throw;
		}
	}

private:
	ModuleService() = default;
	ModuleService(const ModuleService&) = delete;
	ModuleService& operator=(const ModuleService&) = delete;

	std::mutex m_mutex;
};