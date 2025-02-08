#pragma once

#include <string>
#include <vector>
#include <optional>
#include <mutex>
#include <filesystem>
#include "core/Types.h"
#include "core/Config.h"
#include "utils/FileUtils.h"


class ActionService {
public:
	static ActionService& getInstance();

	std::vector<Action> getAllActions();
	std::optional<Action> getAction(const std::string& name);
	void createAction(const Action& action);
	void updateAction(const Action& action);
	bool deleteAction(const std::string& name);

private:
	ActionService() = default;
	ActionService(const ActionService&) = delete;
	ActionService& operator=(const ActionService&) = delete;

	std::mutex m_mutex;
	std::filesystem::path getActionPath(const std::string& name);
};