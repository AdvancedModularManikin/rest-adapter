#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <system_error>

#include "amm/BaseLogger.h"

#include "Exceptions.hpp"

class FileUtils {
public:
	static std::string readFile(const std::filesystem::path& path) {
		try {
			std::ifstream file(path, std::ios::binary);
			if (!file.is_open()) {
				throw FileException("Unable to open file: " + path.string());
			}

			std::stringstream buffer;
			buffer << file.rdbuf();
			return buffer.str();
		}
		catch (const std::exception& e) {
			LOG_ERROR << "Error reading file " << path.string() << ": " << e.what();
			throw FileException(std::string("Failed to read file: ") + e.what());
		}
	}

	static void writeFile(const std::filesystem::path& path, const std::string& content) {
		try {
			// Ensure the directory exists
			std::filesystem::create_directories(path.parent_path());

			// Write to a temporary file first
			auto tempPath = path.string() + ".tmp";
			{
				std::ofstream file(tempPath, std::ios::binary | std::ios::trunc);
				if (!file.is_open()) {
					throw FileException("Unable to create file: " + path.string());
				}
				file.write(content.c_str(), content.length());
				file.flush();
			}

			// Rename temporary file to target file (atomic operation)
			std::filesystem::rename(tempPath, path);
		}
		catch (const std::exception& e) {
			LOG_ERROR << "Error writing file " << path.string() << ": " << e.what();
			throw FileException(std::string("Failed to write file: ") + e.what());
		}
	}

	static void appendToFile(const std::filesystem::path& path, const std::string& content) {
		try {
			std::ofstream file(path, std::ios::app | std::ios::binary);
			if (!file.is_open()) {
				throw FileException("Unable to open file for appending: " + path.string());
			}
			file.write(content.c_str(), content.length());
			file.flush();
		}
		catch (const std::exception& e) {
			LOG_ERROR << "Error appending to file " << path << ": " << e.what();
			throw FileException(std::string("Failed to append to file: ") + e.what());
		}
	}

	static bool deleteFile(const std::filesystem::path& path) {
		try {
			if (!std::filesystem::exists(path)) {
				return false;
			}
			std::filesystem::remove(path);
			return true;
		}
		catch (const std::exception& e) {
			LOG_ERROR << "Error deleting file " << path << ": " << e.what();
			throw FileException(std::string("Failed to delete file: ") + e.what());
		}
	}

	static std::string getLastModifiedTime(const std::filesystem::path& path) {
		try {
			auto ftime = std::filesystem::last_write_time(path);
			auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
					ftime - std::filesystem::file_time_type::clock::now() +
					std::chrono::system_clock::now());

			auto tt = std::chrono::system_clock::to_time_t(sctp);
			std::stringstream ss;
			ss << std::put_time(std::localtime(&tt), "%Y-%m-%d %H:%M:%S");
			return ss.str();
		}
		catch (const std::exception& e) {
			LOG_ERROR << "Error getting last modified time for " << path << ": " << e.what();
			throw FileException(std::string("Failed to get last modified time: ") + e.what());
		}
	}

	static bool exists(const std::filesystem::path& path) {
		return std::filesystem::exists(path);
	}

	static uintmax_t getFileSize(const std::filesystem::path& path) {
		try {
			return std::filesystem::file_size(path);
		}
		catch (const std::exception& e) {
			LOG_ERROR << "Error getting file size for " << path << ": " << e.what();
			throw FileException(std::string("Failed to get file size: ") + e.what());
		}
	}

	static void copyFile(const std::filesystem::path& from, const std::filesystem::path& to) {
		try {
			std::filesystem::create_directories(to.parent_path());
			std::filesystem::copy_file(from, to,
			                           std::filesystem::copy_options::overwrite_existing);
		}
		catch (const std::exception& e) {
			LOG_ERROR << "Error copying file from " << from << " to " << to << ": " << e.what();
			throw FileException(std::string("Failed to copy file: ") + e.what());
		}
	}

	static void moveFile(const std::filesystem::path& from, const std::filesystem::path& to) {
		try {
			std::filesystem::create_directories(to.parent_path());
			std::filesystem::rename(from, to);
		}
		catch (const std::exception& e) {
			LOG_ERROR << "Error moving file from " << from << " to " << to << ": " << e.what();
			throw FileException(std::string("Failed to move file: ") + e.what());
		}
	}

	static std::vector<std::filesystem::path> listFiles(
			const std::filesystem::path& directory,
			const std::string& extension = "") {

		std::vector<std::filesystem::path> files;
		try {
			if (!std::filesystem::exists(directory)) {
				return files;
			}

			for (const auto& entry : std::filesystem::directory_iterator(directory)) {
				if (entry.is_regular_file()) {
					if (extension.empty() || entry.path().extension() == extension) {
						files.push_back(entry.path());
					}
				}
			}
		}
		catch (const std::exception& e) {
			LOG_ERROR << "Error listing files in " << directory << ": " << e.what();
			throw FileException(std::string("Failed to list files: ") + e.what());
		}
		return files;
	}

	static void createDirectories(const std::filesystem::path& path) {
		try {
			std::filesystem::create_directories(path);
		}
		catch (const std::exception& e) {
			LOG_ERROR << "Error creating directories " << path << ": " << e.what();
			throw FileException(std::string("Failed to create directories: ") + e.what());
		}
	}

private:
	// Prevent instantiation
	FileUtils() = delete;
	~FileUtils() = delete;
	FileUtils(const FileUtils&) = delete;
	FileUtils& operator=(const FileUtils&) = delete;
};