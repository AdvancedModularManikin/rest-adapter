#pragma once

#include <string>
#include <mutex>
#include "core/Types.h"
#include <fstream>
#include <filesystem>
#include "amm/BaseLogger.h"
#include "core/Config.h"
#include "core/MoHSESManager.h"
#include "utils/Exceptions.h"
#include "utils/FileUtils.h"

class SystemService {
public:
	static SystemService& getInstance();

	void initialize();
	InstanceInfo getInstanceInfo();
	void executeCommand(const std::string& payload);
	void executePhysiologyModification(const PhysiologyModification& modData);
	void executeRenderModification(const RenderModification& modData);
	void initiateShutdown();
	void setDebugMode(bool enabled);
	bool isReady() const;

private:
	SystemService();
	SystemService(const SystemService&) = delete;
	SystemService& operator=(const SystemService&) = delete;

	mutable std::mutex m_mutex;
	bool m_initialized;
};