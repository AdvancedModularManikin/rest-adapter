#pragma once

#include <string>
#include <map>
#include <optional>
#include <mutex>
#include "managers/DataManagers.hpp"
#include <sstream>
#include "core/MoHSESManager.h"
#include "utils/Exceptions.h"
#include "utils/FileUtils.h"

class StatusService {
public:
	static StatusService& getInstance();

	std::map<std::string, std::string> getAllStatus();
	std::optional<std::string> getStatusValue(const std::string& key);
	std::map<std::string, double> getAllNodes();
	std::optional<double> getNodeValue(const std::string& name);
	std::string getLabsReport();
	void updateStatus(const std::string& key, const std::string& value);
	void updateNode(const std::string& name, double value);
	void resetLabs();
	void appendLabRow();

private:
	StatusService() = default;
	StatusService(const StatusService&) = delete;
	StatusService& operator=(const StatusService&) = delete;

	std::mutex m_mutex;

	NodeDataManager& m_nodeDataManager = NodeDataManager::getInstance();
	StatusManager& m_statusManager = StatusManager::getInstance();
	LabsManager& m_labsManager = LabsManager::getInstance();

	std::string createLabsHeader();
	std::string createLabRow();
};