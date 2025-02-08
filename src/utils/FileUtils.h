#pragma once

#include <string>
#include <vector>
#include <filesystem>

#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include "amm/BaseLogger.h"
#include "Exceptions.h"

class FileUtils {
public:
	static std::string readFile(const std::filesystem::path& path);
	static void writeFile(const std::filesystem::path& path, const std::string& content);
	static void appendToFile(const std::filesystem::path& path, const std::string& content);
	static bool deleteFile(const std::filesystem::path& path);
	static std::string getLastModifiedTime(const std::filesystem::path& path);
	static bool exists(const std::filesystem::path& path);
	static uintmax_t getFileSize(const std::filesystem::path& path);
	static void copyFile(const std::filesystem::path& from, const std::filesystem::path& to);
	static void moveFile(const std::filesystem::path& from, const std::filesystem::path& to);
	static std::vector<std::filesystem::path> listFiles(
			const std::filesystem::path& directory,
			const std::string& extension = "");
	static void createDirectories(const std::filesystem::path& path);

private:
	FileUtils() = delete;
	~FileUtils() = delete;
	FileUtils(const FileUtils&) = delete;
	FileUtils& operator=(const FileUtils&) = delete;
};