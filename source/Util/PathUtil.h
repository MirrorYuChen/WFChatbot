/*
 * @Author: chenjingyu
 * @Date: 2026-03-02 16:58:03
 * @Contact: 2458006466@qq.com
 * @Description: PathUtil
 */
#pragma once

#include "Api.h"

#include <string>

NAMESPACE_BEGIN
class API PathUtil {
public:
  static bool isDir(const std::string &path);
  static bool isFile(const std::string &path);
  static std::string ConcatPath(const std::string &lhs, const std::string &rhs);
  /**
   * 返回最后一级名字
   * /home/user/test.txt -> text.txt
   * /home/user/ -> user
   */
  static std::string Base(const std::string &path);
  static std::string Suffix(const std::string &path);
};
NAMESPACE_END
