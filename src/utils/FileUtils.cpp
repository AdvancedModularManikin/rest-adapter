#include "FileUtils.h"


std::string FileUtils::readFile(const std::filesystem::path& path) {
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

void FileUtils::writeFile(const std::filesystem::path& path, const std::string& content) {
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

void FileUtils::appendToFile(const std::filesystem::path& path, const std::string& content) {
	try {
		std::ofstream file(path, std::ios::app | std::ios::binary);
		if (!file.is_open()) {
			throw FileException("Unable to open file for appending: " + path.string());
		}
		file.write(content.c_str(), content.length());
		file.flush();
	}
	catch (const std::exception& e) {
		LOG_ERROR << "Error appending to file " << path.string() << ": " << e.what();
		throw FileException(std::string("Failed to append to file: ") + e.what());
	}
}

bool FileUtils::deleteFile(const std::filesystem::path& path) {
	try {
		if (!std::filesystem::exists(path)) {
			return false;
		}
		std::filesystem::remove(path);
		return true;
	}
	catch (const std::exception& e) {
		LOG_ERROR << "Error deleting file " << path.string() << ": " << e.what();
		throw FileException(std::string("Failed to delete file: ") + e.what());
	}
}

std::string FileUtils::getLastModifiedTime(const std::filesystem::path& path) {
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
		LOG_ERROR << "Error getting last modified time for " << path.string() << ": " << e.what();
		throw FileException(std::string("Failed to get last modified time: ") + e.what());
	}
}

bool FileUtils::exists(const std::filesystem::path& path) {
	return std::filesystem::exists(path);
}

uintmax_t FileUtils::getFileSize(const std::filesystem::path& path) {
	try {
		return std::filesystem::file_size(path);
	}
	catch (const std::exception& e) {
		LOG_ERROR << "Error getting file size for " << path.string() << ": " << e.what();
		throw FileException(std::string("Failed to get file size: ") + e.what());
	}
}

void FileUtils::copyFile(const std::filesystem::path& from, const std::filesystem::path& to) {
	try {
		std::filesystem::create_directories(to.parent_path());
		std::filesystem::copy_file(from, to,
		                           std::filesystem::copy_options::overwrite_existing);
	}
	catch (const std::exception& e) {
		LOG_ERROR << "Error copying file from " << from.string() << " to " << to.string() << ": " << e.what();
		throw FileException(std::string("Failed to copy file: ") + e.what());
	}
}

void FileUtils::moveFile(const std::filesystem::path& from, const std::filesystem::path& to) {
	try {
		std::filesystem::create_directories(to.parent_path());
		std::filesystem::rename(from, to);
	}
	catch (const std::exception& e) {
		LOG_ERROR << "Error moving file from " << from.string() << " to " << to.string() << ": " << e.what();
		throw FileException(std::string("Failed to move file: ") + e.what());
	}
}

std::vector<std::filesystem::path> FileUtils::listFiles(
		const std::filesystem::path& directory,
		const std::string& extension) {
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
		LOG_ERROR << "Error listing files in " << directory.string() << ": " << e.what();
		throw FileException(std::string("Failed to list files: ") + e.what());
	}
	return files;
}

void FileUtils::createDirectories(const std::filesystem::path& path) {
	try {
		std::filesystem::create_directories(path);
	}
	catch (const std::exception& e) {
		LOG_ERROR << "Error creating directories " << path.string() << ": " << e.what();
		throw FileException(std::string("Failed to create directories: ") + e.what());
	}
}