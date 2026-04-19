/*
 * @Author: chenjingyu
 * @Date: 2026-03-02 16:16:02
 * @Contact: 2458006466@qq.com
 * @Description: PathUtil
 */
#pragma once

#include "Api.h"

#include <string>

NAMESPACE_BEGIN
class API FileUtil {
public:
  static int size(const std::string &path, size_t *size);

  static bool FileExists(const std::string &path);

  static bool CreateDirectories(const std::string &path);

  static bool RemoveDirectory(const std::string &path);

  static bool CreateFileWithSize(const std::string &path, size_t size);
};

NAMESPACE_END
