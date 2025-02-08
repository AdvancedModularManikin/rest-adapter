#pragma once

#include <memory>
#include <string>
#include <mutex>
#include <chrono>
#include <atomic>
#include <optional>

#include "plog/Log.h"
#include "Config.hpp"
#include "core/Types.hpp"
#include "utils/Exceptions.hpp"
#include "managers/NodeDataManager.hpp"
#include "managers/StatusManager.hpp"
#include "managers/LabsManager.hpp"

#include "amm/DDSManager.h"
#include "amm/BaseLogger.h"
#include "amm_std.h"

class MoHSESListener {
public:
	MoHSESListener() : m_lastTick(0) {}

	void onNewStatus(AMM::Status& st, SampleInfo_t* info) {
		std::ostringstream statusValue;
		statusValue << AMM::Utility::EStatusValueStr(st.value());

		auto& statusManager = StatusManager::getInstance();

		if (st.module_name() == "AMM_FluidManager" && st.capability().empty()) {
			statusManager.set("FLUIDICS_STATE", statusValue.str());
		}

		if (st.module_name() == "AMM_FluidManager" && st.capability() == "clear_supply") {
			statusManager.set("CLEAR_SUPPLY", statusValue.str());
		}

		if (st.module_name() == "AMM_FluidManager" && st.capability() == "blood_supply") {
			statusManager.set("BLOOD_SUPPLY", statusValue.str());
		}

		if (st.capability() == "iv_detection") {
			statusManager.set("IVARM_STATE", statusValue.str());
		}
	}

	void onNewTick(AMM::Tick& t, SampleInfo_t* info) {
		auto& statusManager = StatusManager::getInstance();

		if (statusManager.get("STATUS") == "NOT RUNNING" && t.frame() > getLastTick()) {
			statusManager.set("STATUS", "RUNNING");
		}

		setLastTick(t.frame());
		statusManager.set("TICK", std::to_string(t.frame()));
		statusManager.set("TIME", std::to_string(t.time()));
	}

	void onNewCommand(AMM::Command& c, SampleInfo_t* info) {
		const std::string sysPrefix = "[SYS]";
		auto& statusManager = StatusManager::getInstance();
		auto& nodeManager = NodeDataManager::getInstance();
		auto& labsManager = LabsManager::getInstance();

		if (!c.message().compare(0, sysPrefix.size(), sysPrefix)) {
			std::string value = c.message().substr(sysPrefix.size());
			if (value == "START_SIM") {
				statusManager.set("STATUS", "RUNNING");
			} else if (value == "STOP_SIM") {
				statusManager.set("STATUS", "STOPPED");
			} else if (value == "PAUSE_SIM") {
				statusManager.set("STATUS", "PAUSED");
			} else if (value == "RESET_SIM") {
				statusManager.set("STATUS", "NOT RUNNING");
				statusManager.set("TICK", "0");
				statusManager.set("TIME", "0");
				nodeManager.clear();
				labsManager.clear();
			}
		}
	}

	void onNewPhysiologyValue(AMM::PhysiologyValue& n, SampleInfo_t* info) {
		NodeDataManager::getInstance().set(n.name(), n.value());
	}

	int64_t getLastTick() const {
		return m_lastTick.load(std::memory_order_acquire);
	}

	void setLastTick(int64_t tick) {
		m_lastTick.store(tick, std::memory_order_release);
	}

private:
	std::atomic<int64_t> m_lastTick;
};

class MoHSESManager {
public:
	static MoHSESManager& getInstance() {
		static MoHSESManager instance;
		return instance;
	}

	void initialize(const std::string& configFile) {
		std::lock_guard<std::mutex> lock(m_mutex);

		if (m_initialized) {
			LOG_WARNING << "MoHSES Manager already initialized";
			return;
		}

		try {
			m_manager = std::make_unique<AMM::DDSManager<MoHSESListener>>(configFile);
			initializeTopics();
			initializeSubscribers();
			initializePublishers();

			m_uuid.id(m_manager->GenerateUuidString());
			publishOperationalDescription();
			publishConfiguration();

			m_initialized = true;
		}
		catch (const std::exception& e) {
			LOG_ERROR << "Failed to initialize MoHSES Manager: " << e.what();
			throw MoHSESManagerException(e.what());
		}
	}

	void shutdown() {
		std::lock_guard<std::mutex> lock(m_mutex);
		if (!m_initialized) return;

		try {
			m_manager = nullptr;
			m_initialized = false;
			LOG_INFO << "MoHSES Manager shutdown complete";
		}
		catch (const std::exception& e) {
			LOG_ERROR << "Error during MoHSES Manager shutdown: " << e.what();
			throw MoHSESManagerException(e.what());
		}
	}

	AMM::DDSManager<MoHSESListener>& getManager() {
		checkInitialized();
		return *m_manager;
	}

	const AMM::DDSManager<MoHSESListener>& getManager() const {
		checkInitialized();
		return *m_manager;
	}

	void sendCommand(const std::string& command) {
		checkInitialized();
		try {
			if (command.compare(0, m_sysPrefix.size(), m_sysPrefix) == 0) {
				handleSystemCommand(command);
			} else {
				AMM::Command cmdInstance;
				cmdInstance.message(command);
				m_manager->WriteCommand(cmdInstance);
			}
		}
		catch (const std::exception& e) {
			LOG_ERROR << "Error sending command: " << e.what();
			throw MoHSESManagerException(e.what());
		}
	}

	AMM::UUID sendEventRecord(const std::string& location,
	                          const std::string& practitioner,
	                          const std::string& type) {
		checkInitialized();
		try {
			AMM::EventRecord er;
			AMM::FMA_Location fmaL;
			fmaL.name(location);
			er.type(type);
			er.location(fmaL);

			AMM::UUID eventUUID;
			eventUUID.id(m_manager->GenerateUuidString());
			er.id(eventUUID);

			m_manager->WriteEventRecord(er);
			return eventUUID;
		}
		catch (const std::exception& e) {
			LOG_ERROR << "Error sending event record: " << e.what();
			throw MoHSESManagerException(e.what());
		}
	}

	void sendPhysiologyModification(const AMM::UUID& er_id,
	                                const std::string& type,
	                                const std::string& payload) {
		checkInitialized();
		try {
			AMM::PhysiologyModification modInstance;
			AMM::UUID instUUID;
			instUUID.id(m_manager->GenerateUuidString());

			modInstance.id(instUUID);
			modInstance.type(type);
			modInstance.event_id(er_id);
			modInstance.data(payload);

			m_manager->WritePhysiologyModification(modInstance);
			LOG_INFO << "Published physiology modification: " << type;
		}
		catch (const std::exception& e) {
			LOG_ERROR << "Error sending physiology modification: " << e.what();
			throw MoHSESManagerException(e.what());
		}
	}

	void sendRenderModification(const AMM::UUID& er_id,
	                            const std::string& type,
	                            const std::string& payload) {
		checkInitialized();
		try {
			AMM::RenderModification modInstance;
			AMM::UUID instUUID;
			instUUID.id(m_manager->GenerateUuidString());

			modInstance.id(instUUID);
			modInstance.type(type);
			modInstance.event_id(er_id);
			modInstance.data(payload);

			m_manager->WriteRenderModification(modInstance);
			LOG_INFO << "Published render modification: " << type;
		}
		catch (const std::exception& e) {
			LOG_ERROR << "Error sending render modification: " << e.what();
			throw MoHSESManagerException(e.what());
		}
	}

	const AMM::UUID& getModuleUuid() const {
		return m_uuid;
	}

private:
	MoHSESManager() = default;
	~MoHSESManager() { shutdown(); }
	MoHSESManager(const MoHSESManager&) = delete;
	MoHSESManager& operator=(const MoHSESManager&) = delete;

	mutable std::mutex m_mutex;
	bool m_initialized = false;
	std::unique_ptr<AMM::DDSManager<MoHSESListener>> m_manager;
	MoHSESListener m_listener;
	AMM::UUID m_uuid;

	const std::string m_moduleName = "MoHSES_REST_Adapter";
	const std::string m_sysPrefix = "[SYS]";
	const std::string m_manufacturer = "Vcom3D";
	const std::string m_model = "REST Adapter";
	const std::string m_version = "1.0.0";

	enum class SimulationState {
		NOT_RUNNING,
		RUNNING,
		PAUSED,
		STOPPED
	};

	SimulationState m_currentState = SimulationState::NOT_RUNNING;

	bool canTransitionTo(SimulationState newState) const {
		switch (m_currentState) {
			case SimulationState::NOT_RUNNING:
				return newState == SimulationState::RUNNING;
			case SimulationState::RUNNING:
				return newState == SimulationState::PAUSED ||
				       newState == SimulationState::STOPPED;
			case SimulationState::PAUSED:
				return newState == SimulationState::RUNNING ||
				       newState == SimulationState::STOPPED;
			case SimulationState::STOPPED:
				return newState == SimulationState::NOT_RUNNING;
			default:
				return false;
		}
	}

	void transitionTo(SimulationState newState) {
		if (!canTransitionTo(newState)) {
			throw MoHSESManagerException("Invalid state transition");
		}
		m_currentState = newState;
	}

	void checkInitialized() const {
		if (!m_initialized) {
			throw MoHSESManagerException("Manager not initialized");
		}
	}

	void initializeTopics() {
		m_manager->InitializeCommand();
		m_manager->InitializeSimulationControl();
		m_manager->InitializePhysiologyValue();
		m_manager->InitializeTick();
		m_manager->InitializeEventRecord();
		m_manager->InitializeRenderModification();
		m_manager->InitializePhysiologyModification();
		m_manager->InitializeAssessment();
		m_manager->InitializeOperationalDescription();
		m_manager->InitializeModuleConfiguration();
		m_manager->InitializeStatus();
	}

	void initializeSubscribers() {
		m_manager->CreateTickSubscriber(&m_listener, &MoHSESListener::onNewTick);
		m_manager->CreatePhysiologyValueSubscriber(&m_listener, &MoHSESListener::onNewPhysiologyValue);
		m_manager->CreateCommandSubscriber(&m_listener, &MoHSESListener::onNewCommand);
		m_manager->CreateStatusSubscriber(&m_listener, &MoHSESListener::onNewStatus);
	}

	void initializePublishers() {
		m_manager->CreateAssessmentPublisher();
		m_manager->CreateRenderModificationPublisher();
		m_manager->CreatePhysiologyModificationPublisher();
		m_manager->CreateSimulationControlPublisher();
		m_manager->CreateCommandPublisher();
		m_manager->CreateOperationalDescriptionPublisher();
		m_manager->CreateModuleConfigurationPublisher();
		m_manager->CreateStatusPublisher();
	}

	void handleSystemCommand(const std::string& command) {
		std::string value = command.substr(m_sysPrefix.size());
		AMM::SimulationControl simControl;
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::system_clock::now().time_since_epoch()
		).count();
		simControl.timestamp(ms);

		if (value == "START_SIM") {
			simControl.type(AMM::ControlType::RUN);
		}
		else if (value == "STOP_SIM") {
			simControl.type(AMM::ControlType::HALT);
		}
		else if (value == "PAUSE_SIM") {
			simControl.type(AMM::ControlType::HALT);
		}
		else if (value == "RESET_SIM") {
			simControl.type(AMM::ControlType::RESET);
		}
		else if (value == "SAVE_STATE") {
			simControl.type(AMM::ControlType::SAVE);
		}
		else {
			AMM::Command cmdInstance;
			cmdInstance.message(command);
			m_manager->WriteCommand(cmdInstance);
			return;
		}

		m_manager->WriteSimulationControl(simControl);
	}

	void publishOperationalDescription() {
		AMM::OperationalDescription od;
		od.name(m_moduleName);
		od.model(m_model);
		od.manufacturer(m_manufacturer);
		od.serial_number(m_version);
		od.module_id(m_uuid);
		od.module_version(m_version);

		const std::string capabilities = AMM::Utility::read_file_to_string(
				Config::getInstance().getConfigPath() + "rest_adapter_capabilities.xml"
		);
		od.capabilities_schema(capabilities);
		od.description();

		m_manager->WriteOperationalDescription(od);
	}

	void publishConfiguration() {
		AMM::ModuleConfiguration mc;
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::system_clock::now().time_since_epoch()
		).count();

		mc.timestamp(ms);
		mc.module_id(m_uuid);
		mc.name(m_moduleName);

		const std::string configuration = AMM::Utility::read_file_to_string(
				Config::getInstance().getConfigPath() + "rest_adapter_configuration.xml"
		);
		mc.capabilities_configuration(configuration);

		m_manager->WriteModuleConfiguration(mc);
	}
};