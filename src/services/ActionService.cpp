#include "ActionService.h"

ActionService& ActionService::getInstance() {
	static ActionService instance;
	return instance;
}

std::vector<Action> ActionService::getAllActions() {
	std::lock_guard<std::mutex> lock(m_mutex);
	std::vector<Action> actions;

	auto actionPath = Config::getInstance().getActionPath();
	for (const auto& entry : std::filesystem::directory_iterator(actionPath)) {
		if (entry.is_regular_file()) {
			Action action;
			action.name = entry.path().filename().string();
			action.lastModified = FileUtils::getLastModifiedTime(entry.path());
			actions.push_back(action);
		}
	}

	return actions;
}

std::optional<Action> ActionService::getAction(const std::string& name) {
	std::lock_guard<std::mutex> lock(m_mutex);

	auto path = getActionPath(name);
	if (!std::filesystem::exists(path)) {
		return std::nullopt;
	}

	Action action;
	action.name = name;
	action.content = FileUtils::readFile(path);
	action.lastModified = FileUtils::getLastModifiedTime(path);

	return action;
}

void ActionService::createAction(const Action& action) {
	std::lock_guard<std::mutex> lock(m_mutex);

	auto path = getActionPath(action.name);
	if (std::filesystem::exists(path)) {
		throw std::runtime_error("Action already exists: " + action.name);
	}

	FileUtils::writeFile(path, action.content);
}

void ActionService::updateAction(const Action& action) {
	std::lock_guard<std::mutex> lock(m_mutex);

	auto path = getActionPath(action.name);
	if (!std::filesystem::exists(path)) {
		throw std::runtime_error("Action not found: " + action.name);
	}

	FileUtils::writeFile(path, action.content);
}

bool ActionService::deleteAction(const std::string& name) {
	std::lock_guard<std::mutex> lock(m_mutex);

	auto path = getActionPath(name);
	if (!std::filesystem::exists(path)) {
		return false;
	}

	std::filesystem::remove(path);
	return true;
}

std::filesystem::path ActionService::getActionPath(const std::string& name) {
	return std::filesystem::path(Config::getInstance().getActionPath()) / name;
}