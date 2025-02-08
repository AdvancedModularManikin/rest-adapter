#include "SystemService.h"

SystemService::SystemService() : m_initialized(false) {}

SystemService& SystemService::getInstance() {
	static SystemService instance;
	return instance;
}

void SystemService::initialize() {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_initialized = true;
}

InstanceInfo SystemService::getInstanceInfo() {
	std::lock_guard<std::mutex> lock(m_mutex);
	try {
		char hostname[HOST_NAME_MAX];
		gethostname(hostname, HOST_NAME_MAX);

		std::string scenario;
		try {
			std::ifstream file("static/current_scenario.txt");
			if (file) {
				std::getline(file, scenario);
			}
		}
		catch (const std::exception& e) {
			LOG_WARNING << "Failed to read current scenario: " << e.what();
		}

		return InstanceInfo{hostname, scenario};
	}
	catch (const std::exception& e) {
		LOG_ERROR << "Failed to get instance info: " << e.what();
		throw ValidationException("Failed to get instance info: " + std::string(e.what()));
	}
}

void SystemService::executeCommand(const std::string& payload) {
	std::lock_guard<std::mutex> lock(m_mutex);
	try {
		MoHSESManager::getInstance().sendCommand(payload);
	}
	catch (const std::exception& e) {
		LOG_ERROR << "Failed to execute command: " << e.what();
		throw ValidationException("Failed to execute command: " + std::string(e.what()));
	}
}

void SystemService::executePhysiologyModification(const PhysiologyModification& modData) {
	std::lock_guard<std::mutex> lock(m_mutex);
	try {
		// Create event record and get UUID
		auto eventUUID = MoHSESManager::getInstance().sendEventRecord(
				modData.location,
				modData.practitioner,
				modData.type
		);

		// Send physiology modification
		MoHSESManager::getInstance().sendPhysiologyModification(
				eventUUID,
				modData.type,
				modData.payload
		);
	}
	catch (const std::exception& e) {
		LOG_ERROR << "Failed to execute physiology modification: " << e.what();
		throw ValidationException("Failed to execute physiology modification: " +
		                          std::string(e.what()));
	}
}

void SystemService::executeRenderModification(const RenderModification& modData) {
	std::lock_guard<std::mutex> lock(m_mutex);
	try {
		// Create event record and get UUID
		auto eventUUID = MoHSESManager::getInstance().sendEventRecord(
				modData.location,
				modData.practitioner,
				modData.type
		);

		// Send render modification
		MoHSESManager::getInstance().sendRenderModification(
				eventUUID,
				modData.type,
				modData.payload
		);
	}
	catch (const std::exception& e) {
		LOG_ERROR << "Failed to execute render modification: " << e.what();
		throw ValidationException("Failed to execute render modification: " +
		                          std::string(e.what()));
	}
}

void SystemService::initiateShutdown() {
	std::lock_guard<std::mutex> lock(m_mutex);
	try {
		MoHSESManager::getInstance().sendCommand("[SYS]SHUTDOWN");
	}
	catch (const std::exception& e) {
		LOG_ERROR << "Failed to initiate shutdown: " << e.what();
		throw ValidationException("Failed to initiate shutdown: " + std::string(e.what()));
	}
}

void SystemService::setDebugMode(bool enabled) {
	std::lock_guard<std::mutex> lock(m_mutex);
	try {
		Config::getInstance().setDebugEnabled(enabled);
	}
	catch (const std::exception& e) {
		LOG_ERROR << "Failed to set debug mode: " << e.what();
		throw ValidationException("Failed to set debug mode: " + std::string(e.what()));
	}
}

bool SystemService::isReady() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	try {
		return Config::getInstance().isInitialized() && m_initialized;
	}
	catch (const std::exception& e) {
		LOG_ERROR << "Failed to check ready status: " << e.what();
		return false;
	}
}