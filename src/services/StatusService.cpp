#include "StatusService.h"

StatusService& StatusService::getInstance() {
	static StatusService instance;
	return instance;
}

std::map<std::string, std::string> StatusService::getAllStatus() {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_statusManager.getAll();
}

std::optional<std::string> StatusService::getStatusValue(const std::string& key) {
	std::lock_guard<std::mutex> lock(m_mutex);
	auto value = m_statusManager.get(key);
	return value.empty() ? std::nullopt : std::optional<std::string>(value);
}

std::map<std::string, double> StatusService::getAllNodes() {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_nodeDataManager.getAll();
}

std::optional<double> StatusService::getNodeValue(const std::string& name) {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_nodeDataManager.get(name);
}

std::string StatusService::getLabsReport() {
	std::lock_guard<std::mutex> lock(m_mutex);
	auto labs = m_labsManager.getAll();
	if (labs.empty()) {
		return "";
	}

	std::string result;
	for (size_t i = 0; i < labs.size(); ++i) {
		result += labs[i];
		if (i < labs.size() - 1) {
			result += "\n";
		}
	}
	return result;
}

void StatusService::updateStatus(const std::string& key, const std::string& value) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_statusManager.set(key, value);
}

void StatusService::updateNode(const std::string& name, double value) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_nodeDataManager.set(name, value);
}

void StatusService::resetLabs() {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_labsManager.clear();
	// Initialize with header row
	std::string header = createLabsHeader();
	m_labsManager.append(header);
}

void StatusService::appendLabRow() {
	std::lock_guard<std::mutex> lock(m_mutex);
	std::string row = createLabRow();
	m_labsManager.append(row);
}

std::string StatusService::createLabsHeader() {
	std::ostringstream header;
	header << "Time,";
	// Add all column headers here
	return header.str();
}

std::string StatusService::createLabRow() {
	std::ostringstream row;
	auto nodeData = m_nodeDataManager.getAll();

	// Add time
	row << nodeData["SIM_TIME"] << ",";
	// Add all row data here
	return row.str();
}