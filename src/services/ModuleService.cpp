#include "ModuleService.h"

ModuleService& ModuleService::getInstance() {
	static ModuleService instance;
	return instance;
}

std::vector<Module> ModuleService::getAllModules() {
	std::lock_guard<std::mutex> lock(m_mutex);
	try {
		return QueryExecutor::getInstance().getModules();
	}
	catch (const DatabaseException& e) {
		LOG_ERROR << "Failed to get all modules: " << e.what();
		throw;
	}
}

ModuleCounts ModuleService::getModuleCounts() {
	std::lock_guard<std::mutex> lock(m_mutex);
	try {
		return QueryExecutor::getInstance().getModuleCounts();
	}
	catch (const DatabaseException& e) {
		LOG_ERROR << "Failed to get module counts: " << e.what();
		throw;
	}
}

std::vector<std::string> ModuleService::getOtherModules() {
	std::lock_guard<std::mutex> lock(m_mutex);
	try {
		return QueryExecutor::getInstance().getOtherModules();
	}
	catch (const DatabaseException& e) {
		LOG_ERROR << "Failed to get other modules: " << e.what();
		throw;
	}
}

std::optional<Module> ModuleService::getModuleById(const std::string& id) {
	std::lock_guard<std::mutex> lock(m_mutex);
	try {
		return QueryExecutor::getInstance().getModuleById(id);
	}
	catch (const DatabaseException& e) {
		LOG_ERROR << "Failed to get module by ID " << id << ": " << e.what();
		throw;
	}
}

std::optional<Module> ModuleService::getModuleByGuid(const std::string& guid) {
	std::lock_guard<std::mutex> lock(m_mutex);
	try {
		return QueryExecutor::getInstance().getModuleByGuid(guid);
	}
	catch (const DatabaseException& e) {
		LOG_ERROR << "Failed to get module by GUID " << guid << ": " << e.what();
		throw;
	}
}

std::vector<EventLogEntry> ModuleService::getEventLog() {
	std::lock_guard<std::mutex> lock(m_mutex);
	try {
		return QueryExecutor::getInstance().getEventLog();
	}
	catch (const DatabaseException& e) {
		LOG_ERROR << "Failed to get event log: " << e.what();
		throw;
	}
}

std::vector<DiagnosticLogEntry> ModuleService::getDiagnosticLog() {
	std::lock_guard<std::mutex> lock(m_mutex);
	try {
		return QueryExecutor::getInstance().getDiagnosticLog();
	}
	catch (const DatabaseException& e) {
		LOG_ERROR << "Failed to get diagnostic log: " << e.what();
		throw;
	}
}