/*
 * @Author: chenjingyu
 * @Date: 2026-04-19 03:10:49
 * @Contact: 2458006466@qq.com
 * @Description: ChatbotServer
 */
#include "Server/ChatbotServer.h"

#include "Agent/Agent.h"
#include "Base/Json.h"
#include "Base/Logger.h"
#include "LLM/LLMDashScope.h"
#include "LLM/LLMDeepSeek.h"
#include "LLM/LLMOpenAI.h"
#include "Server/Callback.h"

#include <functional>
#include <regex>
#include <sstream>

#include <workflow/RedisMessage.h>

using namespace protocol;

NAMESPACE_BEGIN
ChatbotServer::ChatbotServer(const ChatbotServerConfig &cfg)
    : cfg_(cfg),
      redis_url_(cfg.redis_url),
      server_(std::bind(&ChatbotServer::Process, this, std::placeholders::_1)) {
}

ChatbotServer::~ChatbotServer() { Stop(); }

void ChatbotServer::SetupLLM() {
  FormatInfo("llm_model_type: {}, llm_api_key present: {}", cfg_.llm_model_type,
             cfg_.llm_api_key.empty() ? "false" : "true");
  if (cfg_.llm_api_key.empty()) {
    const char *env_key = nullptr;
    if (cfg_.llm_model_type == "deepseek") {
      env_key = std::getenv("DEEPSEEK_API_KEY");
      FormatInfo("Trying DEEPSEEK_API_KEY: {}",
                 env_key ? "found" : "not found");
    } else if (cfg_.llm_model_type == "openai") {
      env_key = std::getenv("OPENAI_API_KEY");
      FormatInfo("Trying OPENAI_API_KEY: {}", env_key ? "found" : "not found");
    } else {
      env_key = std::getenv("DASHSCOPE_API_KEY");
      FormatInfo("Trying DASHSCOPE_API_KEY: {}",
                 env_key ? "found" : "not found");
    }
    if (env_key) {
      cfg_.llm_api_key = env_key;
    }
  }

  Json llm_cfg = Json::Object{};
  if (!cfg_.llm_api_key.empty()) {
    llm_cfg["api_key"] = cfg_.llm_api_key;
  }
  if (!cfg_.llm_api_base.empty()) {
    llm_cfg["api_base"] = cfg_.llm_api_base;
  }
  llm_cfg["model"] = cfg_.llm_model;
  llm_cfg["model_type"] = cfg_.llm_model_type;
  const Json &llm_cfg_ref = llm_cfg;

  LLMBase *llm = nullptr;
  if (cfg_.llm_model_type == "deepseek") {
    FormatInfo("Creating LLMDeepSeek");
    llm = new LLMDeepSeek(llm_cfg_ref);
  } else if (cfg_.llm_model_type == "openai") {
    FormatInfo("Creating LLMOpenAI");
    llm = new LLMOpenAI(llm_cfg_ref);
  } else {
    FormatInfo("Creating LLMDashScope");
    llm = new LLMDashScope(llm_cfg_ref);
  }
  Assistant::AssistantConfig agent_cfg;
  agent_cfg.llm = std::unique_ptr<LLMBase>(llm);
  agent_cfg.tools = cfg_.tools;
  agent_cfg.tool_configs = cfg_.tool_configs;
  agent_cfg.system_message = cfg_.system_message;
  agent_ = std::make_unique<Assistant>(std::move(agent_cfg));
}

bool ChatbotServer::Start() {
  struct WFGlobalSettings settings = *WFGlobal::get_global_settings();
  settings.endpoint_params.use_tls_sni = true;
  WORKFLOW_library_init(&settings);

  SetupLLM();

  if (server_.start(cfg_.port) == 0) {
    StreamInfo << "ChatBot Service started on port " << cfg_.port;
    StreamInfo << "Available endpoints:";
    StreamInfo << "GET  /health                 - Check server status";
    StreamInfo << "GET  /v1/models              - List available models";
    StreamInfo
        << "POST /v1/chat/completions    - Chat with AI (streaming supported)";
    StreamInfo << "GET  /v1/context/{user_id}   - Get user context history";
    StreamInfo << "DELETE /v1/context/{user_id} - Clear user context";
    StreamInfo << "";
    StreamInfo << "Example usage:";
    StreamInfo << "  Non-streaming: curl -X POST http://localhost:" << cfg_.port
               << "/v1/chat/completions -H 'Content-Type: application/json' "
               << "-d '{\"user_id\":\"user123\",\"messages\":[{\"role\":\"user\",\"content\":\"Hello!\"}]}'";
    StreamInfo << "  Streaming:      curl -N -X POST http://localhost:" << cfg_.port
               << "/v1/chat/completions -H 'Content-Type: application/json' "
               << "-d '{\"user_id\":\"user123\",\"stream\":true,\"messages\":[{\"role\":\"user\",\"content\":\"Hello!\"}]}'";
    return true;
  }
  return false;
}

void ChatbotServer::Stop() { server_.stop(); }

void ChatbotServer::Process(WFHttpTask *task) {
  REGEX_FUNC(HelloWorld, "GET", "/hello_world", task);
  REGEX_FUNC(getHealth, "GET", "/health", task);
  REGEX_FUNC(getModels, "GET", "/v1/models", task);
  REGEX_FUNC(ChatCompletions, "POST", "/v1/chat/completions", task,
             agent_.get(), redis_url_, model_);
  REGEX_FUNC(getContext, "GET", "/v1/context/.*", task, redis_url_);
  REGEX_FUNC(ClearContext, "DELETE", "/v1/context/.*", task, redis_url_);
}

bool CheckRequest(WFHttpTask *task, const std::string &methold,
                  const std::string &uri) {
  auto *req = task->get_req();
  auto req_uri = req->get_request_uri();
  auto req_methold = req->get_method();

  std::stringstream pattern;
  pattern << "^" << methold << "$";
  std::regex method_regex(pattern.str(), std::regex_constants::icase);
  std::regex uri_regex(uri);
  return std::regex_match(req_methold, method_regex) &&
         std::regex_match(req_uri, uri_regex);
}

NAMESPACE_END
