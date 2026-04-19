/*
 * @Author: chenjingyu
 * @Date: 2026-02-27 17:55:04
 * @Contact: 2458006466@qq.com
 * @Description: Base64
 */
#pragma once

#include "Api.h"

#include <string>

NAMESPACE_BEGIN
class API Base64 {
public:
  static std::string encode(const unsigned char *bytes_to_encode,
                            unsigned int bytes_len);

  static std::string decode(std::string const &encoded_string);

private:
  static const std::string base64_chars;
};

NAMESPACE_END
