/*
 * @Author: chenjingyu
 * @Date: 2026-04-18 21:39:03
 * @Contact: 2458006466@qq.com
 * @Description: ToolBuildin
 */
#pragma once

#include "Api.h"
#include "Base/Json.h"
#include "Tools/ToolBase.h"

#include <cstdlib>
#include <ctime>
#include <sstream>

NAMESPACE_BEGIN
// Web Search Tool
class API ToolWebSearch : public ToolBase {
public:
  explicit ToolWebSearch(const Json &cfg = Json()) : cfg_(cfg) {}
  std::string name() const override;
  std::string description() const override;

  Json parameters() const override;

  ToolResult Call(const std::string &params,
                  const Json &kwargs = Json()) override;

private:
  Json cfg_;
};

// Current Time Tool
class API ToolCurrentTime : public ToolBase {
public:
  explicit ToolCurrentTime(const Json &cfg = Json()) : cfg_(cfg) {}
  std::string name() const override;
  std::string description() const override;

  Json parameters() const override;

  ToolResult Call(const std::string &params,
                  const Json &kwargs = Json()) override;

private:
  Json cfg_;
};

// Calculator Tool
class API ToolCalculator : public ToolBase {
public:
  explicit ToolCalculator(const Json &cfg = Json()) : cfg_(cfg) {}
  std::string name() const override;
  std::string description() const override;

  Json parameters() const override;

  ToolResult Call(const std::string &params,
                  const Json &kwargs = Json()) override;

private:
  Json cfg_;
};
NAMESPACE_END
