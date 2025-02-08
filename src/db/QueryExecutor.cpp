#include "QueryExecutor.h"


QueryExecutor& QueryExecutor::getInstance() {
	static QueryExecutor instance;
	return instance;
}

QueryExecutor::QueryExecutor() : m_db(DatabaseConnection()) {}

std::vector<Module> QueryExecutor::getModules() {
	std::vector<Module> modules;
	try {
		m_db.executeQuery(Queries::GET_MODULES,
		                  [&modules](std::string id, std::string name,
		                             std::string desc, std::string caps,
		                             std::string manf, std::string model) {
			                  Module module;
			                  module.id = id;
			                  module.name = name;
			                  module.description = desc;
			                  module.capabilities = caps;
			                  module.manufacturer = manf;
			                  module.model = model;
			                  modules.push_back(module);
		                  });
	}
	catch (const std::exception& e) {
		LOG_ERROR << "Failed to retrieve modules: " << e.what();
		throw DatabaseException("Failed to retrieve modules");
	}
	return modules;
}

std::optional<Module> QueryExecutor::getModuleById(const std::string& id) {
	std::optional<Module> result;
	try {
		m_db.executeQuery(Queries::GET_MODULE_BY_ID,
		                  [&result](std::string moduleId, std::string name,
		                            std::string desc, std::string caps,
		                            std::string manf, std::string model) {
			                  Module module;
			                  module.id = moduleId;
			                  module.name = name;
			                  module.description = desc;
			                  module.capabilities = caps;
			                  module.manufacturer = manf;
			                  module.model = model;
			                  result = module;
		                  }, id);
	}
	catch (const std::exception& e) {
		LOG_ERROR << "Failed to retrieve module by ID: " << e.what();
		throw DatabaseException("Failed to retrieve module by ID");
	}
	return result;
}

std::optional<Module> QueryExecutor::getModuleByGuid(const std::string& guid) {
	std::optional<Module> result;
	try {
		m_db.executeQuery(Queries::GET_MODULE_BY_GUID,
		                  [&result](std::string id, std::string moduleGuid,
		                            std::string name, std::string caps,
		                            std::string manf, std::string model) {
			                  Module module;
			                  module.id = id;
			                  module.guid = moduleGuid;
			                  module.name = name;
			                  module.capabilities = caps;
			                  module.manufacturer = manf;
			                  module.model = model;
			                  result = module;
		                  }, guid);
	}
	catch (const std::exception& e) {
		LOG_ERROR << "Failed to retrieve module by GUID: " << e.what();
		throw DatabaseException("Failed to retrieve module by GUID");
	}
	return result;
}

ModuleCounts QueryExecutor::getModuleCounts() {
	ModuleCounts counts{0, 0, 0};
	try {
		m_db.executeQuery(Queries::GET_MODULE_COUNT,
		                  [&counts](int total, int core) {
			                  counts.total = total;
			                  counts.core = core;
			                  counts.other = total - core;
		                  });
	}
	catch (const std::exception& e) {
		LOG_ERROR << "Failed to retrieve module counts: " << e.what();
		throw DatabaseException("Failed to retrieve module counts");
	}
	return counts;
}

std::vector<std::string> QueryExecutor::getOtherModules() {
	std::vector<std::string> modules;
	try {
		m_db.executeQuery(Queries::GET_OTHER_MODULES,
		                  [&modules](std::string moduleName) {
			                  modules.push_back(moduleName);
		                  });
	}
	catch (const std::exception& e) {
		LOG_ERROR << "Failed to retrieve other modules: " << e.what();
		throw DatabaseException("Failed to retrieve other modules");
	}
	return modules;
}

std::vector<EventLogEntry> QueryExecutor::getEventLog() {
	std::vector<EventLogEntry> entries;
	try {
		m_db.executeQuery(Queries::GET_EVENT_LOG,
		                  [&entries](std::string moduleId, std::string moduleName,
		                             std::string source, std::string topic,
		                             int64_t timestamp, std::string data) {
			                  EventLogEntry entry;
			                  entry.moduleId = moduleId;
			                  entry.moduleName = moduleName;
			                  entry.source = source;
			                  entry.topic = topic;
			                  entry.timestamp = timestamp;
			                  entry.data = data;
			                  entries.push_back(entry);
		                  });
	}
	catch (const std::exception& e) {
		LOG_ERROR << "Failed to retrieve event log: " << e.what();
		throw DatabaseException("Failed to retrieve event log");
	}
	return entries;
}

std::vector<DiagnosticLogEntry> QueryExecutor::getDiagnosticLog() {
	std::vector<DiagnosticLogEntry> entries;
	try {
		m_db.executeQuery(Queries::GET_DIAGNOSTIC_LOG,
		                  [&entries](std::string moduleName, std::string moduleGuid,
		                             std::string moduleId, std::string message,
		                             std::string logLevel, int64_t timestamp) {
			                  DiagnosticLogEntry entry;
			                  entry.moduleName = moduleName;
			                  entry.moduleGuid = moduleGuid;
			                  entry.moduleId = moduleId;
			                  entry.message = message;
			                  entry.logLevel = logLevel;
			                  entry.timestamp = timestamp;
			                  entries.push_back(entry);
		                  });
	}
	catch (const std::exception& e) {
		LOG_ERROR << "Failed to retrieve diagnostic log: " << e.what();
		throw DatabaseException("Failed to retrieve diagnostic log");
	}
	return entries;
}