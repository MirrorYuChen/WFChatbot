/*
 * @Author: chenjingyu
 * @Date: 2026-04-20 10:34:51
 * @Contact: 2458006466@qq.com
 * @Description: ToolMCP
 */
#pragma once

#include "Base/Json.h"
#include "Tools/ToolBase.h"
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

NAMESPACE_BEGIN

class MCPProcess;

// Singleton to manage MCP servers
class API MCPManager {
public:
  static MCPManager &instance();

  // Add a new MCP server and register its tools
  bool AddServer(const std::string &name, const std::string &command,
                 const std::vector<std::string> &args,
                 const std::map<std::string, std::string> &env = {});

  struct ToolInfo {
    std::string server_name;
    std::string tool_name;
    std::string description;
    Json parameters;
  };

  // Execute an MCP tool call
  ToolResult CallTool(const std::string &server_name,
                      const std::string &tool_name, const std::string &params);

private:
  MCPManager() = default;
  ~MCPManager() = default;

  std::map<std::string, std::shared_ptr<MCPProcess>> servers_;
  std::mutex mutex_;
  std::atomic<int> request_id_counter_{1};
};

// Tool implementation for a specific MCP tool
class API ToolMCP : public ToolBase {
public:
  ToolMCP(const std::string &server_name, const std::string &tool_name,
          const std::string &desc, const Json &params_schema)
      : server_name_(server_name), tool_name_(tool_name), desc_(desc),
        params_schema_(params_schema) {}

  std::string name() const override {
    return "mcp_" + server_name_ + "_" + tool_name_;
  }
  std::string description() const override { return desc_; }
  Json parameters() const override { return params_schema_; }

  ToolResult Call(const std::string &params,
                  const Json &kwargs = Json()) override {
    return MCPManager::instance().CallTool(server_name_, tool_name_, params);
  }

private:
  std::string server_name_;
  std::string tool_name_;
  std::string desc_;
  Json params_schema_;
};

NAMESPACE_END
