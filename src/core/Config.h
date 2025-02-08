#pragma once

#include <string>
#include <mutex>
#include <filesystem>
#include <fstream>

#include <rapidjson/document.h>

#include "amm/BaseLogger.h"
#include "utils/Exceptions.h"
#include "utils/FileUtils.h"


class Config {
public:
	// Server configuration defaults
	static constexpr int DEFAULT_PORT = 9080;
	static constexpr int DEFAULT_THREAD_COUNT = 2;
	static constexpr int DEFAULT_DB_POOL_SIZE = 5;
	static constexpr const char* DEFAULT_BIND_ADDRESS = "*";
	static constexpr const char* DEFAULT_DATABASE_PATH = "mohses.db";
	static constexpr const char* DEFAULT_MOHSES_CONFIG = "mohses_config.xml";

	static constexpr const char* DEFAULT_CONFIG_PATH = "./config/";
	static constexpr const char* DEFAULT_ACTION_PATH = "./Actions/";
	static constexpr const char* DEFAULT_ASSESSMENT_PATH = "./assessments/";
	static constexpr const char* DEFAULT_STATE_PATH = "./states/";
	static constexpr const char* DEFAULT_PATIENT_PATH = "./patients/";
	static constexpr const char* DEFAULT_SCENARIO_PATH = "./static/scenarios/";

	static Config& getInstance();

	void initialize(const std::string& configPath);

	// Server configuration getters
	std::string getBindAddress() const;
	int getPort() const;
	int getThreadCount() const;
	int getDbPoolSize() const;

	// Core configuration getters
	std::string getDatabasePath() const;
	std::string getMoHSESConfigFile() const;

	std::string getActionPath() const;
	std::string getAssessmentPath() const;
	std::string getConfigPath() const;
	std::string getStatePath() const;
	std::string getPatientPath() const;
	std::string getScenarioPath() const;

	// Feature flags
	bool isDaemonized() const;
	bool isDiscoveryEnabled() const;
	bool isDebugEnabled() const;

	// Runtime configuration updates
	void setDaemonized(bool value);
	void setDiscoveryEnabled(bool value);
	void setDebugEnabled(bool value);

	bool isInitialized() const;

private:
	Config() = default;
	~Config() = default;
	Config(const Config&) = delete;
	Config& operator=(const Config&) = delete;

	mutable std::recursive_mutex m_mutex;
	bool m_initialized = false;
	rapidjson::Document m_config;

	std::string getPath(const char* key, const std::string& defaultValue) const;
	void loadConfigFile(const std::string& configPath);
	void validateConfiguration();

	// Helper methods for getting typed values with defaults
	std::string getString(const char* key, const std::string& defaultValue) const;
	int getInt(const char* key, int defaultValue) const;
	bool getBool(const char* key, bool defaultValue) const;
	void setBool(const char* key, bool value);
};