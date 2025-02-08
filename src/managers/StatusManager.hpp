#pragma once

#include <map>
#include <mutex>
#include <string>

#include "amm/BaseLogger.h"

class StatusManager {
public:
	static StatusManager& getInstance() {
		static StatusManager instance;
		return instance;
	}

	StatusManager() {
		m_storage = {
				{"STATUS", "NOT RUNNING"},
				{"TICK", "0"},
				{"TIME", "0"},
				{"SCENARIO", ""},
				{"STATE", ""},
				{"CLEAR_SUPPLY", ""},
				{"BLOOD_SUPPLY", ""},
				{"FLUIDICS_STATE", ""},
				{"IVARM_STATE", ""}
		};
	}

	void set(const std::string& key, const std::string& value) {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_storage[key] = value;
	}

	std::string get(const std::string& key) const {
		std::lock_guard<std::mutex> lock(m_mutex);
		auto it = m_storage.find(key);
		return it != m_storage.end() ? it->second : "";
	}

	std::map<std::string, std::string> getAll() const {
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_storage;
	}

private:
	StatusManager(const StatusManager&) = delete;
	StatusManager& operator=(const StatusManager&) = delete;

	mutable std::mutex m_mutex;
	std::map<std::string, std::string> m_storage;
};
