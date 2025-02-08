#pragma once

#include <vector>
#include <mutex>
#include <string>

#include "plog/Log.h"

class LabsManager {
public:
	static LabsManager& getInstance() {
		static LabsManager instance;
		return instance;
	}

	void clear() {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_storage.clear();
	}

	void append(const std::string& row) {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_storage.push_back(row);
	}

	std::vector<std::string> getAll() const {
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_storage;
	}

private:
	LabsManager() = default;
	LabsManager(const LabsManager&) = delete;
	LabsManager& operator=(const LabsManager&) = delete;

	mutable std::mutex m_mutex;
	std::vector<std::string> m_storage;
};