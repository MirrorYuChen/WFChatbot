/*
 * @Author: chenjingyu
 * @Date: 2026-04-18 21:32:34
 * @Contact: 2458006466@qq.com
 * @Description: ToolBase
 */
#pragma once

#include "Api.h"
#include "Base/Json.h"
#include "LLM/LLMSchema.h"

#include <functional>
#include <map>
#include <memory>
#include <string>

NAMESPACE_BEGIN
// Tool output type
struct API ToolResult {
  std::string text;
  std::string error;
  bool success = true;

  ToolResult() = default;
  explicit ToolResult(const std::string &t) : text(t), success(true) {}
  static ToolResult error_result(const std::string &msg) {
    ToolResult r;
    r.error = msg;
    r.success = false;
    return r;
  }
};

// Base tool class - abstract interface for all tools
class API ToolBase {
public:
  ToolBase();
  virtual ~ToolBase() = default;

  // Tool metadata (to be set by subclasses)
  virtual std::string name() const = 0;
  virtual std::string description() const = 0;
  virtual Json parameters() const = 0;

  // Execute the tool with parameters
  virtual ToolResult Call(const std::string &params,
                          const Json &kwargs = Json()) = 0;

  // Get tool definition as JSON (OpenAI function schema)
  Json getFunctionDef() const;

  // Validate parameters against schema
  bool ValidateParams(const std::string &params, std::string &error_msg) const;

protected:
  // Helper to parse JSON params
  Json ParseParams(const std::string &params) const;

  // Helper to check if required params present
  bool CheckRequired(const Json &params,
                      const std::vector<std::string> &required) const;
};

// Tool registry for dynamic tool registration
class API ToolRegistry {
public:
  static ToolRegistry &instance();

  using ToolCreator = std::function<ToolBase *(const Json &)>;

  void RegisterTool(const std::string &name, ToolCreator creator);
  ToolBase *CreateTool(const std::string &name, const Json &cfg = Json());
  bool HasTool(const std::string &name) const;
  std::vector<std::string> ListTools() const;

private:
  std::map<std::string, ToolCreator> creators_;
};

// Helper macro to register LLM types
#define REGISTERTOOL(name, cls)                                                \
  static bool __register##cls = []() {                                        \
    ToolRegistry::instance().RegisterTool(                                      \
        name, [](const Json &cfg) -> ToolBase * { return new cls(cfg); });      \
    return true;                                                               \
  }();

NAMESPACE_END
