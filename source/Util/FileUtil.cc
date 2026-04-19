/*
 * @Author: chenjingyu
 * @Date: 2026-03-02 16:16:07
 * @Contact: 2458006466@qq.com
 * @Description: FileUtil
 */
#include "Util/FileUtil.h"

#include <filesystem>
#include <random>
#include <fstream>

namespace fs = std::filesystem;

NAMESPACE_BEGIN
int FileUtil::size(const std::string &path, size_t *size) {
  int status = 0;
  *size = 0;

  try {
    fs::file_status ss = fs::status(path);
    if (fs::is_regular_file(ss)) {
      *size = fs::file_size(path);
    } else {
      status = -1;
    }
  } catch (const fs::filesystem_error &e) {
    if (e.code() == std::make_error_code(std::errc::no_such_file_or_directory)) {
      status = -1;
    } else {
      status = -1;
    }
  }
  return status;
}

bool FileUtil::FileExists(const std::string &path) {
  return fs::exists(path) && fs::is_regular_file(path);
}

bool FileUtil::CreateDirectories(const std::string &path) {
  try {
    fs::create_directories(path);
    return true;
  } catch (const fs::filesystem_error &e) {
    if (e.code() == std::make_error_code(std::errc::file_exists)) {
      return true;
    } else {
      return false;
    }
  }
}

bool FileUtil::RemoveDirectory(const std::string &path) {
  try {
    if (fs::exists(path)) {
      fs::remove_all(path);
      return true;
    }
    return false;
  } catch (const fs::filesystem_error &e) {
    return false;
  }
}

bool FileUtil::CreateFileWithSize(const std::string &path, size_t size) {
  try {
    fs::path file_path(path);
    if (!file_path.parent_path().empty()) {
      fs::create_directories(file_path.parent_path());
    }
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) {
      return false;
    }
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(32, 126);

    const size_t buffer_size = 4096;
    std::vector<char> buffer(buffer_size);
    size_t remaining = size;

    while (remaining > 0) {
      size_t chunk_size = std::min(remaining, buffer_size);
      for (size_t i = 0; i < chunk_size; ++i) {
        buffer[i] = static_cast<char>(distrib(gen));
      }
      ofs.write(buffer.data(), chunk_size);
      if (!ofs) {
        return false;
      }
      remaining -= chunk_size;
    }

    ofs.close();
    return true;
  } catch (const std::exception &e) {
    return false;
  }
}

NAMESPACE_END
