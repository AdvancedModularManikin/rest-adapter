#include "MoHSESManager.h"
#include <chrono>
#include "Config.h"
#include "utils/Exceptions.h"
#include "amm/BaseLogger.h"

MoHSESManager::MoHSESManager() = default;

MoHSESManager::~MoHSESManager() {
	shutdown();
}

MoHSESManager& MoHSESManager::getInstance() {
	static MoHSESManager instance;
	return instance;
}

void MoHSESManager::initialize(const std::string& configFile) {
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

void MoHSESManager::shutdown() {
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

AMM::DDSManager<MoHSESListener>& MoHSESManager::getManager() {
	checkInitialized();
	return *m_manager;
}

const AMM::DDSManager<MoHSESListener>& MoHSESManager::getManager() const {
	checkInitialized();
	return *m_manager;
}

void MoHSESManager::sendCommand(const std::string& command) {
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

AMM::UUID MoHSESManager::sendEventRecord(const std::string& location,
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

void MoHSESManager::sendPhysiologyModification(const AMM::UUID& er_id,
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

void MoHSESManager::sendRenderModification(const AMM::UUID& er_id,
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

const AMM::UUID& MoHSESManager::getModuleUuid() const {
	return m_uuid;
}

void MoHSESManager::checkInitialized() const {
	if (!m_initialized) {
		throw MoHSESManagerException("Manager not initialized");
	}
}

bool MoHSESManager::canTransitionTo(SimulationState newState) const {
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

void MoHSESManager::transitionTo(SimulationState newState) {
	if (!canTransitionTo(newState)) {
		throw MoHSESManagerException("Invalid state transition");
	}
	m_currentState = newState;
}

void MoHSESManager::initializeTopics() {
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

void MoHSESManager::initializeSubscribers() {
	m_manager->CreateTickSubscriber(&m_listener, &MoHSESListener::onNewTick);
	m_manager->CreatePhysiologyValueSubscriber(&m_listener, &MoHSESListener::onNewPhysiologyValue);
	m_manager->CreateCommandSubscriber(&m_listener, &MoHSESListener::onNewCommand);
	m_manager->CreateStatusSubscriber(&m_listener, &MoHSESListener::onNewStatus);
}

void MoHSESManager::initializePublishers() {
	m_manager->CreateAssessmentPublisher();
	m_manager->CreateRenderModificationPublisher();
	m_manager->CreatePhysiologyModificationPublisher();
	m_manager->CreateSimulationControlPublisher();
	m_manager->CreateCommandPublisher();
	m_manager->CreateOperationalDescriptionPublisher();
	m_manager->CreateModuleConfigurationPublisher();
	m_manager->CreateStatusPublisher();
}

void MoHSESManager::handleSystemCommand(const std::string& command) {
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

void MoHSESManager::publishOperationalDescription() {
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

void MoHSESManager::publishConfiguration() {
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