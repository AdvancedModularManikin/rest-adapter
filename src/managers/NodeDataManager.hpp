#pragma once

#include <map>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>

#include "amm/BaseLogger.h"

class NodeDataManager {
public:
	static NodeDataManager& getInstance() {
		static NodeDataManager instance;
		return instance;
	}

	void clear() {
		std::unique_lock<std::shared_mutex> lock(m_mutex);
		m_storage.clear();
	}

	void set(const std::string& key, double value) {
		std::unique_lock<std::shared_mutex> lock(m_mutex);
		if (!std::isnan(value)) {
			m_storage[key] = value;
		}
	}

	std::optional<double> get(const std::string& key) const {
		std::shared_lock<std::shared_mutex> lock(m_mutex);
		auto it = m_storage.find(key);
		if (it != m_storage.end()) {
			return it->second;
		}
		return std::nullopt;
	}

	std::map<std::string, double> getAll() const {
		std::shared_lock<std::shared_mutex> lock(m_mutex);
		return m_storage;
	}

private:
	NodeDataManager() = default;
	NodeDataManager(const NodeDataManager&) = delete;
	NodeDataManager& operator=(const NodeDataManager&) = delete;

	mutable std::shared_mutex m_mutex;
	std::map<std::string, double> m_storage;
};