#include "Config.h"


Config& Config::getInstance() {
	static Config instance;
	return instance;
}

void Config::initialize(const std::string& configPath) {
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

std::string Config::getBindAddress() const {
	return getString("bind_address", DEFAULT_BIND_ADDRESS);
}

int Config::getPort() const {
	return getInt("port", DEFAULT_PORT);
}

int Config::getThreadCount() const {
	return getInt("thread_count", DEFAULT_THREAD_COUNT);
}

int Config::getDbPoolSize() const {
	return getInt("db_pool_size", DEFAULT_DB_POOL_SIZE);
}

std::string Config::getDatabasePath() const {
	return getString("database_path", DEFAULT_DATABASE_PATH);
}

std::string Config::getMoHSESConfigFile() const {
	return getString("mohses_config", DEFAULT_MOHSES_CONFIG);
}

std::string Config::getActionPath() const {
	return getPath("action_path", DEFAULT_ACTION_PATH);
}

std::string Config::getAssessmentPath() const {
	return getPath("assessment_path", DEFAULT_ASSESSMENT_PATH);
}

std::string Config::getConfigPath() const {
	return getPath("config_path", DEFAULT_CONFIG_PATH);
}

std::string Config::getStatePath() const {
	return getPath("state_path", DEFAULT_STATE_PATH);
}

std::string Config::getPatientPath() const {
	return getPath("patient_path", DEFAULT_PATIENT_PATH);
}

std::string Config::getScenarioPath() const {
	return getPath("scenario_path", DEFAULT_SCENARIO_PATH);
}

bool Config::isDaemonized() const {
	return getBool("daemonize", false);
}

bool Config::isDiscoveryEnabled() const {
	return getBool("discovery_enabled", true);
}

bool Config::isDebugEnabled() const {
	return getBool("debug_enabled", false);
}

void Config::setDaemonized(bool value) {
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	setBool("daemonize", value);
}

void Config::setDiscoveryEnabled(bool value) {
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	setBool("discovery_enabled", value);
}

void Config::setDebugEnabled(bool value) {
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	setBool("debug_enabled", value);
}

bool Config::isInitialized() const {
	return m_initialized;
}

std::string Config::getPath(const char* key, const std::string& defaultValue) const {
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	std::string path = getString(key, defaultValue);
	if (!path.empty() && path.back() != '/' && path.back() != '\\') {
		path += '/';
	}
	return path;
}

void Config::loadConfigFile(const std::string& configPath) {
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

void Config::validateConfiguration() {
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

std::string Config::getString(const char* key, const std::string& defaultValue) const {
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	if (m_config.HasMember(key) && m_config[key].IsString()) {
		return m_config[key].GetString();
	}
	return defaultValue;
}

int Config::getInt(const char* key, int defaultValue) const {
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	if (m_config.HasMember(key) && m_config[key].IsInt()) {
		return m_config[key].GetInt();
	}
	return defaultValue;
}

bool Config::getBool(const char* key, bool defaultValue) const {
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	if (m_config.HasMember(key) && m_config[key].IsBool()) {
		return m_config[key].GetBool();
	}
	return defaultValue;
}

void Config::setBool(const char* key, bool value) {
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