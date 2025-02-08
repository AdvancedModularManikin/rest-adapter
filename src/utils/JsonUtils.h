#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <rapidjson/document.h>
#include "core/Types.h"

class JsonUtils {
public:
	class ParseException : public std::runtime_error {
	public:
		explicit ParseException(const std::string& message);
	};

	// Generic JSON parsing
	static rapidjson::Document parseJson(const std::string& json);

	// Action serialization
	static std::string serializeAction(const Action& action);
	static std::string serializeActions(const std::vector<Action>& actions);
	static Action parseAction(const std::string& json);

	// Module serialization
	static std::string serializeModule(const Module& module);
	static std::string serializeModules(const std::vector<Module>& modules);
	static std::string serializeModuleCounts(const ModuleCounts& counts);
	static std::string serializeModuleNames(const std::vector<std::string>& moduleNames);

	// Event Log serialization
	static std::string serializeEventLog(const std::vector<EventLogEntry>& events);
	static std::string createEventLogCSV(const std::vector<EventLogEntry>& events);

	// Diagnostic Log serialization
	static std::string serializeDiagnosticLog(const std::vector<DiagnosticLogEntry>& logs);
	static std::string createDiagnosticLogCSV(const std::vector<DiagnosticLogEntry>& logs);

	// Error serialization
	static std::string serializeError(const std::string& message, const std::string& details = "");
	static std::string serializeError(const std::string& message,
	                                  const std::string& details,
	                                  const std::map<std::string, std::string>& additionalInfo);

	// Assessment serialization
	static std::string serializeAssessment(const Assessment& assessment);
	static std::string serializeAssessments(const std::vector<Assessment>& assessments);
	static Assessment parseAssessment(const std::string& json);

	// Performance Assessment parsing
	static PerformanceAssessmentData parsePerformanceAssessment(const std::string& json);

	// Status serialization
	static std::string serializeStatus(const std::map<std::string, std::string>& status);

	// Key-Value serialization
	static std::string serializeKeyValue(const std::string& key, const std::string& value);
	static std::string serializeKeyValue(const std::string& key, double value);

	// Nodes and Status combined serialization
	static std::string serializeNodesAndStatus(
			const std::map<std::string, double>& nodes,
			const std::map<std::string, std::string>& status);

	// Instance info serialization
	static std::string serializeInstance(const InstanceInfo& instance);

	// Modification parsing
	static PhysiologyModification parsePhysiologyModification(const std::string& json);
	static RenderModification parseRenderModification(const std::string& json);

private:
	static std::string escapeCSV(const std::string& str);
};