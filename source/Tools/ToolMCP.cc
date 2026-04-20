/*
 * @Author: chenjingyu
 * @Date: 2026-04-20 10:35:05
 * @Contact: 2458006466@qq.com
 * @Description: ToolMCP
 */
#include "Tools/ToolMCP.h"
#include "Base/Logger.h"
#include <fcntl.h>
#include <iostream>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

NAMESPACE_BEGIN

class MCPProcess {
public:
  MCPProcess(const std::string &name, const std::string &cmd,
             const std::vector<std::string> &args)
      : name_(name), cmd_(cmd), args_(args),
        pid_(-1), stdin_pipe_{-1, -1}, stdout_pipe_{-1, -1} {}

  ~MCPProcess() { Stop(); }

  bool Start() {
    if (pipe(stdin_pipe_) == -1 || pipe(stdout_pipe_) == -1) {
      FormatError("[MCP:{}] Failed to create pipes", name_);
      return false;
    }

    pid_ = fork();
    if (pid_ == -1) {
      FormatError("[MCP:{}] Failed to fork", name_);
      return false;
    }

    if (pid_ == 0) { // Child
      dup2(stdin_pipe_[0], STDIN_FILENO);
      dup2(stdout_pipe_[1], STDOUT_FILENO);

      close(stdin_pipe_[0]);
      close(stdin_pipe_[1]);
      close(stdout_pipe_[0]);
      close(stdout_pipe_[1]);

      std::vector<char *> c_args;
      c_args.push_back(const_cast<char *>(cmd_.c_str()));
      for (const auto &arg : args_) {
        c_args.push_back(const_cast<char *>(arg.c_str()));
      }
      c_args.push_back(nullptr);

      execvp(cmd_.c_str(), c_args.data());
      exit(1);
    } else { // Parent
      close(stdin_pipe_[0]);
      close(stdout_pipe_[1]);
      return PerformHandshake();
    }
  }

  void Stop() {
    if (pid_ > 0) {
      kill(pid_, SIGTERM);
      waitpid(pid_, nullptr, 0);
      pid_ = -1;
    }
    if (stdin_pipe_[1] != -1) {
      close(stdin_pipe_[1]);
      stdin_pipe_[1] = -1;
    }
    if (stdout_pipe_[0] != -1) {
      close(stdout_pipe_[0]);
      stdout_pipe_[0] = -1;
    }
  }

  Json Transact(const Json &request) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (pid_ <= 0)
      return Json::Object{};

    // Ignore SIGPIPE for this process to handle write errors gracefully
    static bool sigpipe_ignored = []() {
      signal(SIGPIPE, SIG_IGN);
      return true;
    }();

    std::string req_str = request.dump() + "\n";
    ssize_t nw = write(stdin_pipe_[1], req_str.c_str(), req_str.size());
    if (nw <= 0) {
      FormatError("[MCP:{}] Failed to write to stdin", name_);
      return Json::Object{};
    }

    // Read response line by line using poll for timeout
    std::string response_line;
    struct pollfd pfd;
    pfd.fd = stdout_pipe_[0];
    pfd.events = POLLIN;

    char buf[4096];
    bool found_newline = false;
    while (!found_newline) {
      int ret = poll(&pfd, 1, 10000); // 10 second timeout for complex queries
      if (ret <= 0) {
        FormatError("[MCP:{}] Timeout waiting for response", name_);
        break;
      }

      ssize_t nr = read(stdout_pipe_[0], buf, sizeof(buf));
      if (nr <= 0) {
        FormatError("[MCP:{}] Read failed or EOF", name_);
        break;
      }

      for (ssize_t i = 0; i < nr; ++i) {
        if (buf[i] == '\n') {
          found_newline = true;
          break;
        }
        response_line += buf[i];
      }
    }
    if (response_line.empty()) {
      // Check if process still alive
      int status;
      if (waitpid(pid_, &status, WNOHANG) != 0) {
        FormatError("[MCP:{}] Process died with status {}", name_, status);
        pid_ = -1;
      }
      return Json::Object{};
    }

    try {
      return Json::parse(response_line);
    } catch (...) {
      FormatError("[MCP:{}] Failed to parse JSON: {}", name_, response_line);
      return Json::Object{};
    }
  }

  const std::vector<Json> &GetTools() const { return tools_; }

private:
  bool PerformHandshake() {
    // 1. Initialize
    Json init_req = Json::Object{};
    init_req.push_back("jsonrpc", "2.0");
    init_req.push_back("id", 1);
    init_req.push_back("method", "initialize");
    Json params = Json::Object{};
    params.push_back("protocolVersion", "2024-11-05");
    Json capabilities = Json::Object{};
    params.push_back("capabilities", capabilities);
    Json client_info = Json::Object{};
    client_info.push_back("name", "WFChatbot-MCP-Client");
    client_info.push_back("version", "1.0.0");
    params.push_back("clientInfo", client_info);
    init_req.push_back("params", params);

    Json init_res = Transact(init_req);
    if (!init_res.has("result")) {
      FormatError("[MCP:{}] Handshake failed: missing result", name_);
      return false;
    }

    // 2. Initialized notification
    Json initialized_notif = Json::Object{};
    initialized_notif.push_back("jsonrpc", "2.0");
    initialized_notif.push_back("method", "notifications/initialized");
    std::string notif_str = initialized_notif.dump() + "\n";
    if (pid_ > 0) {
      ssize_t nw = write(stdin_pipe_[1], notif_str.c_str(), notif_str.size());
      (void)nw;
    }

    // 3. List Tools
    Json list_req = Json::Object{};
    list_req.push_back("jsonrpc", "2.0");
    list_req.push_back("id", 2);
    list_req.push_back("method", "tools/list");

    Json list_res = Transact(list_req);
    if (list_res.has("result") && list_res["result"].has("tools")) {
      Json tools = list_res["result"]["tools"];
      if (tools.is_array()) {
        for (size_t i = 0; i < tools.size(); ++i) {
          tools_.push_back(tools[i].copy());
        }
      }
      return true;
    }
    FormatError("[MCP:{}] Failed to list tools", name_);
    return false;
  }

  std::string name_;
  std::string cmd_;
  std::vector<std::string> args_;
  pid_t pid_;
  int stdin_pipe_[2];
  int stdout_pipe_[2];
  std::vector<Json> tools_;
  std::mutex mutex_;
};

MCPManager &MCPManager::instance() {
  static MCPManager instance;
  return instance;
}

bool MCPManager::AddServer(const std::string &name, const std::string &command,
                           const std::vector<std::string> &args,
                           const std::map<std::string, std::string> &env) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto server = std::make_shared<MCPProcess>(name, command, args);
  if (!server->Start())
    return false;

  servers_[name] = server;

  // Register each tool found
  for (const auto &tool_def : server->GetTools()) {
    std::string tool_name = static_cast<std::string>(tool_def["name"]);
    std::string desc = static_cast<std::string>(tool_def["description"]);
    Json params = tool_def["inputSchema"];

    std::string register_name = "mcp_" + name + "_" + tool_name;
    ToolRegistry::instance().RegisterTool(
        register_name, [name, tool_name, desc, params](const Json &cfg) {
          return new ToolMCP(name, tool_name, desc, params);
        });
    FormatInfo("[MCP] Registered tool: {}", register_name);
  }

  return true;
}

ToolResult MCPManager::CallTool(const std::string &server_name,
                                const std::string &tool_name,
                                const std::string &params) {
  std::shared_ptr<MCPProcess> server;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = servers_.find(server_name);
    if (it == servers_.end())
      return ToolResult::error_result("Server not found");
    server = it->second;
  }

  Json call_req = Json::Object{};
  call_req.push_back("jsonrpc", "2.0");
  int req_id = request_id_counter_++;
  call_req.push_back("id", req_id);
  call_req.push_back("method", "tools/call");
  Json call_params = Json::Object{};
  call_params.push_back("name", tool_name);
  call_params.push_back("arguments", Json::parse(params));
  call_req.push_back("params", call_params);

  Json res = server->Transact(call_req);
  if (res.has("error")) {
    Json err = res["error"];
    std::string msg = "MCP Error";
    if (err.has("message")) {
      msg += ": " + static_cast<std::string>(err["message"]);
    } else {
      msg += ": " + err.dump();
    }
    return ToolResult::error_result(msg);
  }

  if (res.has("result")) {
    Json result = res["result"];
    bool is_error = false;
    if (result.has("isError") && result["isError"].is_boolean()) {
      is_error = static_cast<bool>(result["isError"]);
    }

    std::string output;
    if (result.has("content") && result["content"].is_array()) {
      Json content = result["content"];
      for (size_t i = 0; i < content.size(); ++i) {
        if (content[i].has("type") &&
            static_cast<std::string>(content[i]["type"]) == "text") {
          if (!output.empty())
            output += "\n";
          output += static_cast<std::string>(content[i]["text"]);
        }
      }
    }

    if (output.empty()) {
      output = result.dump();
    }

    if (is_error) {
      return ToolResult::error_result(output);
    }
    return ToolResult(output);
  }

  return ToolResult::error_result("Unexpected MCP response format (missing result)");
}

NAMESPACE_END
