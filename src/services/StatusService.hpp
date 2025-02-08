#pragma once

#include <string>
#include <map>
#include <vector>
#include <optional>
#include <mutex>

#include "core/MoHSESManager.hpp"
#include "core/Types.hpp"
#include "managers/DataManagers.hpp"

#include "utils/Exceptions.hpp"
#include "utils/FileUtils.hpp"


class StatusService {
public:
	static StatusService &getInstance() {
		static StatusService instance;
		return instance;
	}

	std::map<std::string, std::string> getAllStatus() {
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_statusManager.getAll();
	}

	std::optional<std::string> getStatusValue(const std::string &key) {
		std::lock_guard<std::mutex> lock(m_mutex);
		auto value = m_statusManager.get(key);
		return value.empty() ? std::nullopt : std::optional<std::string>(value);
	}

	std::map<std::string, double> getAllNodes() {
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_nodeDataManager.getAll();
	}

	std::optional<double> getNodeValue(const std::string &name) {
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_nodeDataManager.get(name);
	}

	std::string getLabsReport() {
		std::lock_guard<std::mutex> lock(m_mutex);
		auto labs = m_labsManager.getAll();
		return boost::algorithm::join(labs, "\n");
	}

	void updateStatus(const std::string &key, const std::string &value) {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_statusManager.set(key, value);
	}

	void updateNode(const std::string &name, double value) {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_nodeDataManager.set(name, value);
	}

	void resetLabs() {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_labsManager.clear();
		// Initialize with header row
		std::string header = createLabsHeader();
		m_labsManager.append(header);
	}

	void appendLabRow() {
		std::lock_guard<std::mutex> lock(m_mutex);
		std::string row = createLabRow();
		m_labsManager.append(row);
	}

private:
	StatusService() = default;

	std::mutex m_mutex;

	NodeDataManager &m_nodeDataManager = NodeDataManager::getInstance();
	StatusManager &m_statusManager = StatusManager::getInstance();
	LabsManager &m_labsManager = LabsManager::getInstance();

	std::string createLabsHeader() {
		std::ostringstream header;
		header << "Time,";
		// Add all column headers here
		// ... (add all the column headers as in the original code)
		return header.str();
	}

	std::string createLabRow() {
		std::ostringstream row;
		auto nodeData = m_nodeDataManager.getAll();

		// Add time
		row << nodeData["SIM_TIME"] << ",";
		// Add all row data here
		// ... (add all the row data as in the original code)
		return row.str();
	}
};