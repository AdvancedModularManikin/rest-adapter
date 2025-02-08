#pragma once

#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <mutex>
#include "core/Types.h"
#include "amm_std.h"

#include "core/Config.h"
#include "core/MoHSESManager.h"
#include "utils/FileUtils.h"


class AssessmentService {
public:
	static AssessmentService& getInstance();

	std::vector<Assessment> getAllAssessments();
	std::optional<Assessment> getAssessment(const std::string& name);
	void createAssessment(const Assessment& assessment);
	void updateAssessment(const Assessment& assessment);
	bool deleteAssessment(const std::string& name);
	void sendPerformanceAssessment(const AMM::UUID& eventUUID,
	                               const PerformanceAssessmentData& data);

private:
	AssessmentService() = default;
	AssessmentService(const AssessmentService&) = delete;
	AssessmentService& operator=(const AssessmentService&) = delete;

	std::mutex m_mutex;

	std::filesystem::path getAssessmentPath(const std::string& name);
	void ensureAssessmentDirectory();
};