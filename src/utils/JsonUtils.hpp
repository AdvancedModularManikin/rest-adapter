#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <iomanip>
#include <sstream>
#include <ctime>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/error/en.h>

#include "core/Types.hpp"
#include "utils/Exceptions.hpp"

class JsonUtils {
public:
	class ParseException : public std::runtime_error {
	public:
		explicit ParseException(const std::string& message)
				: std::runtime_error("JSON parse error: " + message) {}
	};

	// Generic JSON parsing
	static rapidjson::Document parseJson(const std::string& json) {
		rapidjson::Document doc;
		rapidjson::ParseResult result = doc.Parse(json.c_str());

		if (result.IsError()) {
			throw ParseException(std::string("Parse error: ") +
			                     rapidjson::GetParseError_En(result.Code()) +
			                     " at offset " + std::to_string(result.Offset()));
		}

		return doc;
	}

	// Action serialization
	static std::string serializeAction(const Action& action) {
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

		writer.StartObject();
		writer.Key("name");
		writer.String(action.name.c_str());
		writer.Key("content");
		writer.String(action.content.c_str());
		writer.Key("last_modified");
		writer.String(action.lastModified.c_str());
		writer.EndObject();

		return buffer.GetString();
	}

	static std::string serializeActions(const std::vector<Action>& actions) {
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

		writer.StartArray();
		for (const auto& action : actions) {
			writer.StartObject();
			writer.Key("name");
			writer.String(action.name.c_str());
			writer.Key("last_modified");
			writer.String(action.lastModified.c_str());
			writer.EndObject();
		}
		writer.EndArray();

		return buffer.GetString();
	}

	static Action parseAction(const std::string& json) {
		auto doc = parseJson(json);

		if (!doc.IsObject()) {
			throw ParseException("JSON root must be an object");
		}

		if (!doc.HasMember("name") || !doc["name"].IsString()) {
			throw ParseException("Missing or invalid 'name' field");
		}

		if (!doc.HasMember("content") || !doc["content"].IsString()) {
			throw ParseException("Missing or invalid 'content' field");
		}

		Action action;
		action.name = doc["name"].GetString();
		action.content = doc["content"].GetString();
		return action;
	}

	// Module serialization
	static std::string serializeModule(const Module& module) {
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

		writer.StartObject();
		writer.Key("module_id");
		writer.String(module.id.c_str());
		if (!module.guid.empty()) {
			writer.Key("module_guid");
			writer.String(module.guid.c_str());
		}
		writer.Key("module_name");
		writer.String(module.name.c_str());
		if (!module.description.empty()) {
			writer.Key("description");
			writer.String(module.description.c_str());
		}
		writer.Key("manufacturer");
		writer.String(module.manufacturer.c_str());
		writer.Key("model");
		writer.String(module.model.c_str());
		if (!module.capabilities.empty()) {
			writer.Key("module_capabilities");
			writer.String(module.capabilities.c_str());
		}
		writer.EndObject();

		return buffer.GetString();
	}

	static std::string serializeModules(const std::vector<Module>& modules) {
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

		writer.StartArray();
		for (const auto& module : modules) {
			writer.StartObject();
			writer.Key("module_id");
			writer.String(module.id.c_str());
			writer.Key("module_name");
			writer.String(module.name.c_str());
			writer.Key("manufacturer");
			writer.String(module.manufacturer.c_str());
			writer.Key("model");
			writer.String(module.model.c_str());
			writer.EndObject();
		}
		writer.EndArray();

		return buffer.GetString();
	}

	static std::string serializeModuleCounts(const ModuleCounts& counts) {
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

		writer.StartObject();
		writer.Key("module_count");
		writer.Int(counts.total);
		writer.Key("core_count");
		writer.Int(counts.core);
		writer.Key("other_count");
		writer.Int(counts.other);
		writer.EndObject();

		return buffer.GetString();
	}

	static std::string serializeModuleNames(const std::vector<std::string>& moduleNames) {
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

		writer.StartArray();
		for (const auto& name : moduleNames) {
			writer.String(name.c_str());
		}
		writer.EndArray();

		return buffer.GetString();
	}

	// Event Log serialization
	static std::string serializeEventLog(const std::vector<EventLogEntry>& events) {
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

		writer.StartArray();
		for (const auto& event : events) {
			writer.StartObject();
			writer.Key("module_id");
			writer.String(event.moduleId.c_str());
			writer.Key("source");
			writer.String(event.moduleName.c_str());
			writer.Key("module_guid");
			writer.String(event.source.c_str());
			writer.Key("timestamp");
			writer.Int64(event.timestamp);
			writer.Key("topic");
			writer.String(event.topic.c_str());
			writer.Key("message");
			writer.String(event.data.c_str());
			writer.EndObject();
		}
		writer.EndArray();

		return buffer.GetString();
	}

	static std::string createEventLogCSV(const std::vector<EventLogEntry>& events) {
		std::ostringstream csv;
		csv << "Timestamp,Module,Source,Topic,Data\n";

		for (const auto& event : events) {
			std::time_t temp = event.timestamp;
			std::tm* t = std::gmtime(&temp);

			csv << std::put_time(t, "%Y-%m-%d %H:%M:%S") << ","
			    << escapeCSV(event.moduleName) << ","
			    << escapeCSV(event.source) << ","
			    << escapeCSV(event.topic) << ","
			    << escapeCSV(event.data) << "\n";
		}

		return csv.str();
	}

	// Diagnostic Log serialization
	static std::string serializeDiagnosticLog(const std::vector<DiagnosticLogEntry>& logs) {
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

		writer.StartArray();
		for (const auto& log : logs) {
			writer.StartObject();
			writer.Key("source");
			writer.String(log.moduleName.c_str());
			writer.Key("module_guid");
			writer.String(log.moduleGuid.c_str());
			writer.Key("module_id");
			writer.String(log.moduleId.c_str());
			writer.Key("timestamp");
			writer.Int64(log.timestamp);
			writer.Key("log_level");
			writer.String(log.logLevel.c_str());
			writer.Key("message");
			writer.String(log.message.c_str());
			writer.EndObject();
		}
		writer.EndArray();

		return buffer.GetString();
	}

	static std::string createDiagnosticLogCSV(const std::vector<DiagnosticLogEntry>& logs) {
		std::ostringstream csv;
		csv << "Timestamp,LogLevel,Module,Message\n";

		for (const auto& log : logs) {
			std::time_t temp = log.timestamp;
			std::tm* t = std::gmtime(&temp);

			csv << std::put_time(t, "%Y-%m-%d %H:%M:%S") << ","
			    << escapeCSV(log.logLevel) << ","
			    << escapeCSV(log.moduleName) << ","
			    << escapeCSV(log.message) << "\n";
		}

		return csv.str();
	}

	static std::string serializeError(const std::string& message,
	                                  const std::string& details = "") {
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

		writer.StartObject();
		writer.Key("error");
		writer.String(message.c_str());
		if (!details.empty()) {
			writer.Key("details");
			writer.String(details.c_str());
		}
		writer.EndObject();

		return buffer.GetString();
	}

	// Extended error response serialization with additional fields
	static std::string serializeError(const std::string& message,
	                                  const std::string& details,
	                                  const std::map<std::string, std::string>& additionalInfo) {
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

		writer.StartObject();
		writer.Key("error");
		writer.String(message.c_str());
		if (!details.empty()) {
			writer.Key("details");
			writer.String(details.c_str());
		}
		if (!additionalInfo.empty()) {
			writer.Key("additional_info");
			writer.StartObject();
			for (const auto& [key, value] : additionalInfo) {
				writer.Key(key.c_str());
				writer.String(value.c_str());
			}
			writer.EndObject();
		}
		writer.EndObject();

		return buffer.GetString();
	}

	static std::string serializeAssessment(const Assessment& assessment) {
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

		writer.StartObject();
		writer.Key("name");
		writer.String(assessment.name.c_str());
		writer.Key("content");
		writer.String(assessment.content.c_str());
		writer.Key("last_modified");
		writer.String(assessment.lastModified.c_str());
		writer.Key("file_size");
		writer.Int64(assessment.fileSize);
		writer.EndObject();

		return buffer.GetString();
	}

	static std::string serializeAssessments(const std::vector<Assessment>& assessments) {
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

		writer.StartArray();
		for (const auto& assessment : assessments) {
			writer.StartObject();
			writer.Key("name");
			writer.String(assessment.name.c_str());
			writer.Key("last_modified");
			writer.String(assessment.lastModified.c_str());
			writer.Key("file_size");
			writer.Int64(assessment.fileSize);
			writer.EndObject();
		}
		writer.EndArray();

		return buffer.GetString();
	}

	static Assessment parseAssessment(const std::string& json) {
		auto doc = parseJson(json);

		if (!doc.IsObject()) {
			throw ParseException("JSON root must be an object");
		}

		if (!doc.HasMember("name") || !doc["name"].IsString()) {
			throw ParseException("Missing or invalid 'name' field");
		}

		if (!doc.HasMember("content") || !doc["content"].IsString()) {
			throw ParseException("Missing or invalid 'content' field");
		}

		Assessment assessment;
		assessment.name = doc["name"].GetString();
		assessment.content = doc["content"].GetString();

		// Optional fields
		if (doc.HasMember("last_modified") && doc["last_modified"].IsString()) {
			assessment.lastModified = doc["last_modified"].GetString();
		}
		if (doc.HasMember("file_size") && doc["file_size"].IsInt64()) {
			assessment.fileSize = doc["file_size"].GetInt64();
		}

		return assessment;
	}

	// Performance Assessment parsing
	static PerformanceAssessmentData parsePerformanceAssessment(const std::string& json) {
		auto doc = parseJson(json);

		if (!doc.IsObject()) {
			throw ParseException("JSON root must be an object");
		}

		PerformanceAssessmentData data;

		if (!doc.HasMember("type") || !doc["type"].IsString()) {
			throw ParseException("Missing or invalid 'type' field");
		}
		data.type = doc["type"].GetString();

		if (doc.HasMember("location") && doc["location"].IsString()) {
			data.location = doc["location"].GetString();
		}
		if (doc.HasMember("practitioner") && doc["practitioner"].IsString()) {
			data.practitioner = doc["practitioner"].GetString();
		}
		if (doc.HasMember("info") && doc["info"].IsString()) {
			data.assessmentInfo = doc["info"].GetString();
		}
		if (doc.HasMember("step") && doc["step"].IsString()) {
			data.step = doc["step"].GetString();
		}
		if (doc.HasMember("comment") && doc["comment"].IsString()) {
			data.comment = doc["comment"].GetString();
		}

		return data;
	}

	// Status serialization
	static std::string serializeStatus(const std::map<std::string, std::string>& status) {
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

		writer.StartObject();
		for (const auto& [key, value] : status) {
			writer.Key(key.c_str());
			writer.String(value.c_str());
		}
		writer.EndObject();

		return buffer.GetString();
	}

	// Key-Value serialization (string version)
	static std::string serializeKeyValue(const std::string& key,
	                                     const std::string& value) {
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

		writer.StartObject();
		writer.Key(key.c_str());
		writer.String(value.c_str());
		writer.EndObject();

		return buffer.GetString();
	}

	// Key-Value serialization (double version)
	static std::string serializeKeyValue(const std::string& key,
	                                     double value) {
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

		writer.StartObject();
		writer.Key(key.c_str());
		writer.Double(value);
		writer.EndObject();

		return buffer.GetString();
	}

	// Nodes and Status combined serialization
	static std::string serializeNodesAndStatus(
			const std::map<std::string, double>& nodes,
			const std::map<std::string, std::string>& status) {

		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

		writer.StartArray();

		// Write nodes
		for (const auto& [key, value] : nodes) {
			writer.StartObject();
			writer.Key(key.c_str());
			writer.Double(value);
			writer.EndObject();
		}

		// Write status
		for (const auto& [key, value] : status) {
			writer.StartObject();
			writer.Key(key.c_str());
			writer.String(value.c_str());
			writer.EndObject();
		}

		writer.EndArray();

		return buffer.GetString();
	}

	// Instance info serialization
	static std::string serializeInstance(const InstanceInfo& instance) {
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

		writer.StartObject();
		writer.Key("name");
		writer.String(instance.name.c_str());
		writer.Key("scenario");
		writer.String(instance.scenario.c_str());
		writer.EndObject();

		return buffer.GetString();
	}

	// Modification parsing
	static PhysiologyModification parsePhysiologyModification(const std::string& json) {
		auto doc = parseJson(json);

		if (!doc.IsObject()) {
			throw ParseException("JSON root must be an object");
		}

		PhysiologyModification data;

		if (!doc.HasMember("payload") || !doc["payload"].IsString()) {
			throw ParseException("Missing or invalid 'payload' field");
		}
		data.payload = doc["payload"].GetString();

		if (doc.HasMember("type") && doc["type"].IsString()) {
			data.type = doc["type"].GetString();
		}
		if (doc.HasMember("location") && doc["location"].IsString()) {
			data.location = doc["location"].GetString();
		}
		if (doc.HasMember("practitioner") && doc["practitioner"].IsString()) {
			data.practitioner = doc["practitioner"].GetString();
		}

		return data;
	}

	static RenderModification parseRenderModification(const std::string& json) {
		auto doc = parseJson(json);

		if (!doc.IsObject()) {
			throw ParseException("JSON root must be an object");
		}

		RenderModification data;

		if (!doc.HasMember("payload") || !doc["payload"].IsString()) {
			throw ParseException("Missing or invalid 'payload' field");
		}
		data.payload = doc["payload"].GetString();

		if (doc.HasMember("type") && doc["type"].IsString()) {
			data.type = doc["type"].GetString();
		}
		if (doc.HasMember("location") && doc["location"].IsString()) {
			data.location = doc["location"].GetString();
		}
		if (doc.HasMember("practitioner") && doc["practitioner"].IsString()) {
			data.practitioner = doc["practitioner"].GetString();
		}

		return data;
	}

private:
	static std::string escapeCSV(const std::string& str) {
		if (str.find(',') != std::string::npos ||
		    str.find('"') != std::string::npos ||
		    str.find('\n') != std::string::npos) {
			std::string escaped = str;
			// Replace any double quotes with two double quotes
			size_t pos = 0;
			while ((pos = escaped.find('"', pos)) != std::string::npos) {
				escaped.replace(pos, 1, "\"\"");
				pos += 2;
			}
			// Wrap in quotes
			return "\"" + escaped + "\"";
		}
		return str;
	}
};