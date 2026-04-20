/*
 * @Author: chenjingyu
 * @Date: 2026-04-19 16:42:46
 * @Contact: 2458006466@qq.com
 * @Description: ExampleChatbot
 */
#include "Server/ChatbotServer.h"
#include "Tools/ToolMCP.h"
#include <workflow/WFFacilities.h>

#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace NAMESPACE;

static WFFacilities::WaitGroup wait_group(1);
// 全局指针，用于信号处理
static std::unique_ptr<ChatbotServer> g_server{nullptr}; 

void sig_handler(int signo) {
  wait_group.done();
  if (g_server) {
    g_server->Stop(); // 在信号处理中停止服务
  }
}

int main(int argc, char *argv[]) {
  signal(SIGINT, sig_handler);
  signal(SIGTERM, sig_handler);

  ChatbotServerConfig cfg;
  cfg.port = 8888;
  cfg.redis_url = "redis://127.0.0.1:6379";
  cfg.llm_model = "deepseek-chat";
  cfg.llm_model_type = "openai";
  const char *api_key = std::getenv("DEEPSEEK_API_KEY");
  if (api_key) {
    cfg.llm_api_key = api_key;
  }
  cfg.llm_api_base = "https://api.deepseek.com/v1";
  cfg.tools.push_back("amap_weather");
  cfg.tools.push_back("calculator");

  // Initialize MCP SQLite
  if (MCPManager::instance().AddServer("sqlite", "mcp-server-sqlite", {"--db-path", "chatbot.db"})) {
    // These names are based on standard server-sqlite tools (read_query, write_query, list_tables, describe_table)
    cfg.tools.push_back("mcp_sqlite_read_query");
    cfg.tools.push_back("mcp_sqlite_write_query");
    cfg.tools.push_back("mcp_sqlite_create_table");
    cfg.tools.push_back("mcp_sqlite_list_tables");
  }

  cfg.system_message =
      "You are a helpful AI assistant with access to various tools.";

  // List all tools currently in registry for debugging
  std::cerr << "Registry Tools: ";
  auto registry_tools = ToolRegistry::instance().ListTools();
  for (size_t i = 0; i < registry_tools.size(); i++) {
    if (i > 0) std::cerr << ", ";
    std::cerr << registry_tools[i];
  }
  std::cerr << "\n";

  g_server.reset(new ChatbotServer(cfg));

  if (g_server->Start()) {
    std::cerr << "Tools: ";
    for (size_t i = 0; i < cfg.tools.size(); i++) {
      if (i > 0)
        std::cerr << ", ";
      std::cerr << cfg.tools[i];
    }
    std::cerr << "\n";
    wait_group.wait();
  } else {
    fprintf(stderr, "Cannot start server\n");
    return 1;
  }
  return 0;
}
