/*
 * @Author: chenjingyu
 * @Date: 2026-03-02 16:58:09
 * @Contact: 2458006466@qq.com
 * @Description: PathUtil
 */
#include "Util/PathUtil.h"

#include <filesystem>

namespace fs = std::filesystem;

NAMESPACE_BEGIN
bool isDir(const std::string &path) {
  try {
    return fs::exists(path) && fs::is_directory(path);
  } catch (const fs::filesystem_error &e) {
    return false;
  }
}

bool isFile(const std::string &path) {
  try {
    return fs::exists(path) && fs::is_regular_file(path);
  } catch (const fs::filesystem_error &e) {
    return false;
  }
}

std::string ConcatPath(const std::string &lhs, const std::string &rhs) {
  try {
    fs::path left(lhs);
    fs::path right(rhs);
    fs::path combined = left / right;
    // 转换成原生路径格式
    return combined.make_preferred().string();
  } catch (const fs::filesystem_error &e) {
    return "";
  }
}

std::string Base(const std::string &path) {
  try {
    fs::path p(path);
    if (p.root_path() == p && !p.empty()) {
      return "/";
    }
    return p.filename().string();
  } catch (const fs::filesystem_error &e) {
    return "";
  }
}

std::string Suffix(const std::string &path) {
  try {
    fs::path p(path);
    std::string ext = p.extension().string();
    if (!ext.empty() && ext.front() == '.') {
      ext = ext.substr(1);
    }
    return ext;
  } catch (const fs::filesystem_error &e) {
    return "";
  }
}

NAMESPACE_END
