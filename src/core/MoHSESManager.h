#pragma once

#include <memory>
#include <string>
#include <mutex>
#include "amm_std.h"
#include "MoHSESListener.h"

class MoHSESManager {
public:
	static MoHSESManager& getInstance();

	void initialize(const std::string& configFile);
	void shutdown();

	AMM::DDSManager<MoHSESListener>& getManager();
	const AMM::DDSManager<MoHSESListener>& getManager() const;

	void sendCommand(const std::string& command);
	AMM::UUID sendEventRecord(const std::string& location,
	                          const std::string& practitioner,
	                          const std::string& type);

	void sendPhysiologyModification(const AMM::UUID& er_id,
	                                const std::string& type,
	                                const std::string& payload);

	void sendRenderModification(const AMM::UUID& er_id,
	                            const std::string& type,
	                            const std::string& payload);

	const AMM::UUID& getModuleUuid() const;

private:
	MoHSESManager();
	~MoHSESManager();
	MoHSESManager(const MoHSESManager&) = delete;
	MoHSESManager& operator=(const MoHSESManager&) = delete;

	enum class SimulationState {
		NOT_RUNNING,
		RUNNING,
		PAUSED,
		STOPPED
	};

	void checkInitialized() const;
	bool canTransitionTo(SimulationState newState) const;
	void transitionTo(SimulationState newState);
	void initializeTopics();
	void initializeSubscribers();
	void initializePublishers();
	void handleSystemCommand(const std::string& command);
	void publishOperationalDescription();
	void publishConfiguration();

	mutable std::mutex m_mutex;
	bool m_initialized = false;
	std::unique_ptr<AMM::DDSManager<MoHSESListener>> m_manager;
	MoHSESListener m_listener;
	AMM::UUID m_uuid;
	SimulationState m_currentState = SimulationState::NOT_RUNNING;

	const std::string m_moduleName = "MoHSES_REST_Adapter";
	const std::string m_sysPrefix = "[SYS]";
	const std::string m_manufacturer = "Vcom3D";
	const std::string m_model = "REST Adapter";
	const std::string m_version = "1.0.0";
};