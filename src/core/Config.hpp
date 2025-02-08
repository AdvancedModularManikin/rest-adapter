#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <optional>

#include "plog/Log.h"
#include "rapidjson/document.h"
#include "rapidjson/istreamwrapper.h"
#include "utils/Exceptions.hpp"
#include "utils/FileUtils.hpp"

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

	static Config& getInstance() {
		static Config instance;
		return instance;
	}

	void initialize(const std::string& configPath) {
		std::lock_guard<std::recursive_mutex> lock(m_mutex);

		if (m_initialized) {
			LOG_WARNING << "Config already initialized";
			return;
		}

		try {
			loadConfigFile(configPath);
			validateConfiguration();
			m_initialized = true;
		}
		catch (const std::exception& e) {
			LOG_ERROR << "Failed to initialize configuration: " << e.what();
			throw ConfigException(e.what());
		}
	}

	// Server configuration getters
	std::string getBindAddress() const { return getString("bind_address", DEFAULT_BIND_ADDRESS); }
	int getPort() const { return getInt("port", DEFAULT_PORT); }
	int getThreadCount() const { return getInt("thread_count", DEFAULT_THREAD_COUNT); }
	int getDbPoolSize() const { return getInt("db_pool_size", DEFAULT_DB_POOL_SIZE); }

	// Core configuration getters
	std::string getDatabasePath() const { return getString("database_path", DEFAULT_DATABASE_PATH); }
	std::string getMoHSESConfigFile() const { return getString("mohses_config", DEFAULT_MOHSES_CONFIG); }

	std::string getActionPath() const { return getPath("action_path", DEFAULT_ACTION_PATH); }
	std::string getAssessmentPath() const { return getPath("assessment_path", DEFAULT_ASSESSMENT_PATH); }
	std::string getConfigPath() const { return getPath("config_path", DEFAULT_CONFIG_PATH); }
	std::string getStatePath() const { return getPath("state_path", DEFAULT_STATE_PATH); }
	std::string getPatientPath() const { return getPath("patient_path", DEFAULT_PATIENT_PATH); }
	std::string getScenarioPath() const { return getPath("scenario_path", DEFAULT_SCENARIO_PATH); }

	// Feature flags
	bool isDaemonized() const { return getBool("daemonize", false); }
	bool isDiscoveryEnabled() const { return getBool("discovery_enabled", true); }
	bool isDebugEnabled() const { return getBool("debug_enabled", false); }

	// Runtime configuration updates
	void setDaemonized(bool value) {
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		setBool("daemonize", value);
	}

	void setDiscoveryEnabled(bool value) {
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		setBool("discovery_enabled", value);
	}

	void setDebugEnabled(bool value) {
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		setBool("debug_enabled", value);
	}

	bool isInitialized() const {
		return m_initialized;
	}

private:
	Config() = default;
	~Config() = default;
	Config(const Config&) = delete;
	Config& operator=(const Config&) = delete;

	mutable std::recursive_mutex m_mutex;
	bool m_initialized = false;
	rapidjson::Document m_config;

	std::string getPath(const char* key, const std::string& defaultValue) const {
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		std::string path = getString(key, defaultValue);
		if (!path.empty() && path.back() != '/' && path.back() != '\\') {
			path += '/';
		}
		return path;
	}

	void loadConfigFile(const std::string& configPath) {
		if (!std::filesystem::exists(configPath)) {
			LOG_WARNING << "Config file not found at " << configPath << ". Using defaults.";
			m_config.SetObject();
			return;
		}

		try {
			std::string jsonContent = FileUtils::readFile(configPath);
			if (m_config.Parse(jsonContent.c_str()).HasParseError()) {
				throw ConfigException("Failed to parse config file: Invalid JSON");
			}
		}
		catch (const std::exception& e) {
			throw ConfigException("Failed to load config file: " + std::string(e.what()));
		}
	}

	void validateConfiguration() {
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		int port = getInt("port", DEFAULT_PORT);
		if (port < 1 || port > 65535) {
			throw ConfigException("Invalid port number: " + std::to_string(port));
		}

		int threads = getInt("thread_count", DEFAULT_THREAD_COUNT);
		if (threads < 1) {
			throw ConfigException("Thread count must be positive: " + std::to_string(threads));
		}

		int poolSize = getInt("db_pool_size", DEFAULT_DB_POOL_SIZE);
		if (poolSize < 1) {
			throw ConfigException("Database pool size must be positive: " + std::to_string(poolSize));
		}

		std::vector<std::string> paths = {
				getActionPath(),
				getAssessmentPath(),
				getConfigPath(),
				getStatePath(),
				getPatientPath(),
				getScenarioPath()
		};

		for (const auto& path : paths) {
			try {
				if (!std::filesystem::exists(path)) {
					std::filesystem::create_directories(path);
				}
			}
			catch (const std::filesystem::filesystem_error& e) {
				throw ConfigException("Failed to create directory " + path + ": " + e.what());
			}
		}

		std::vector<std::string> requiredFiles = {
				getConfigPath() + "rest_adapter_capabilities.xml",
				getConfigPath() + "rest_adapter_configuration.xml"
		};

		for (const auto& file : requiredFiles) {
			if (!std::filesystem::exists(file)) {
				LOG_WARNING << "Required configuration file not found: " << file;
			}
		}
	}

	// Helper methods for getting typed values with defaults
	std::string getString(const char* key, const std::string& defaultValue) const {
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		if (m_config.HasMember(key) && m_config[key].IsString()) {
			return m_config[key].GetString();
		}
		return defaultValue;
	}

	int getInt(const char* key, int defaultValue) const {
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		if (m_config.HasMember(key) && m_config[key].IsInt()) {
			return m_config[key].GetInt();
		}
		return defaultValue;
	}

	bool getBool(const char* key, bool defaultValue) const {
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		if (m_config.HasMember(key) && m_config[key].IsBool()) {
			return m_config[key].GetBool();
		}
		return defaultValue;
	}

	void setBool(const char* key, bool value) {
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		if (!m_config.HasMember(key)) {
			m_config.AddMember(
					rapidjson::Value(key, m_config.GetAllocator()).Move(),
					rapidjson::Value(value).Move(),
					m_config.GetAllocator()
			);
		} else {
			m_config[key].SetBool(value);
		}
	}
};