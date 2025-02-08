#pragma once

#include <string>
#include "amm_std.h"

struct Action {
	std::string name;
	std::string content;
	std::string lastModified;
};

struct Module {
	std::string id;
	std::string guid;
	std::string name;
	std::string description;
	std::string capabilities;
	std::string manufacturer;
	std::string model;
	std::string version;
};

struct ModuleCounts {
	int total;
	int core;
	int other;
};

struct EventLogEntry {
	std::string moduleId;
	std::string moduleName;
	std::string source;
	std::string topic;
	int64_t timestamp;
	std::string data;
};

struct DiagnosticLogEntry {
	std::string moduleName;
	std::string moduleGuid;
	std::string moduleId;
	std::string message;
	std::string logLevel;
	int64_t timestamp;
};

struct Assessment {
	std::string name;
	std::string content;
	std::string filePath;
	std::string lastModified;
	int64_t fileSize;
};

struct PerformanceAssessmentData {
	std::string type;
	std::string location;
	std::string practitioner;
	std::string assessmentInfo;
	std::string step;
	std::string comment;
};

struct InstanceInfo {
	std::string name;
	std::string scenario;
};

struct PhysiologyModification {
	std::string type;
	std::string location;
	std::string practitioner;
	std::string payload;
};

struct RenderModification {
	std::string type;
	std::string location;
	std::string practitioner;
	std::string payload;
};

namespace MoHSES {
	enum class ControlType {
		RUN,
		HALT,
		RESET,
		SAVE
	};

	struct EventRecord {
		AMM::UUID id;
		std::string type;
		std::string location;
		std::string practitioner;
		int64_t timestamp;
	};

	struct ModuleInfo {
		std::string name;
		std::string manufacturer;
		std::string model;
		std::string version;
		AMM::UUID id;
	};


	namespace Status {
		static const std::string NOT_RUNNING = "NOT RUNNING";
		static const std::string RUNNING = "RUNNING";
		static const std::string STOPPED = "STOPPED";
		static const std::string PAUSED = "PAUSED";
	}

}