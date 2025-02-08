#pragma once

#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <mutex>

#include "core/Config.hpp"
#include "core/MoHSESManager.hpp"
#include "core/Types.hpp"

#include "utils/FileUtils.hpp"


class AssessmentService {
public:
	static AssessmentService& getInstance() {
		static AssessmentService instance;
		return instance;
	}

	std::vector<Assessment> getAllAssessments() {
		std::lock_guard<std::mutex> lock(m_mutex);
		std::vector<Assessment> assessments;

		auto assessmentPath = Config::getInstance().getAssessmentPath();
		for (const auto& entry : std::filesystem::directory_iterator(assessmentPath)) {
			if (entry.is_regular_file()) {
				Assessment assessment;
				assessment.name = entry.path().filename().string();
				assessment.filePath = entry.path().string();
				assessment.lastModified = FileUtils::getLastModifiedTime(entry.path());
				assessment.fileSize = entry.file_size();
				assessments.push_back(assessment);
			}
		}

		return assessments;
	}

	std::optional<Assessment> getAssessment(const std::string& name) {
		std::lock_guard<std::mutex> lock(m_mutex);

		auto path = getAssessmentPath(name);
		if (!std::filesystem::exists(path)) {
			return std::nullopt;
		}

		Assessment assessment;
		assessment.name = name;
		assessment.filePath = path.string();
		assessment.content = FileUtils::readFile(path);
		assessment.lastModified = FileUtils::getLastModifiedTime(path);
		assessment.fileSize = std::filesystem::file_size(path);

		return assessment;
	}

	void createAssessment(const Assessment& assessment) {
		std::lock_guard<std::mutex> lock(m_mutex);

		auto path = getAssessmentPath(assessment.name);
		ensureAssessmentDirectory();

		if (std::filesystem::exists(path)) {
			throw std::runtime_error("Assessment already exists: " + assessment.name);
		}

		FileUtils::writeFile(path, assessment.content);
	}

	void updateAssessment(const Assessment& assessment) {
		std::lock_guard<std::mutex> lock(m_mutex);

		auto path = getAssessmentPath(assessment.name);
		if (!std::filesystem::exists(path)) {
			throw std::runtime_error("Assessment not found: " + assessment.name);
		}

		FileUtils::writeFile(path, assessment.content);
	}

	bool deleteAssessment(const std::string& name) {
		std::lock_guard<std::mutex> lock(m_mutex);

		auto path = getAssessmentPath(name);
		if (!std::filesystem::exists(path)) {
			return false;
		}

		std::filesystem::remove(path);
		return true;
	}

	void sendPerformanceAssessment(const AMM::UUID& eventUUID,
	                               const PerformanceAssessmentData& data) {
		AMM::Assessment assessInstance;
		AMM::UUID instUUID;
		instUUID.id(MoHSESManager::getInstance().getModuleUuid().id());

		assessInstance.id(instUUID);
		assessInstance.event_id(eventUUID);
		assessInstance.comment(data.comment);

		// Add any additional assessment data as needed

		MoHSESManager::getInstance().getManager().WriteAssessment(assessInstance);
	}

private:
	AssessmentService() = default;
	std::mutex m_mutex;

	std::filesystem::path getAssessmentPath(const std::string& name) {
		return std::filesystem::path(Config::getInstance().getAssessmentPath()) / name;
	}

	void ensureAssessmentDirectory() {
		auto dir = Config::getInstance().getAssessmentPath();
		if (!std::filesystem::exists(dir)) {
			std::filesystem::create_directories(dir);
		}
	}
};