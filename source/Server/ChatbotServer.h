/*
 * @Author: chenjingyu
 * @Date: 2026-04-19 03:10:42
 * @Contact: 2458006466@qq.com
 * @Description: ChatbotServer
 */
#pragma once

#include "Api.h"
#include "Base/Json.h"

#include <workflow/WFFacilities.h>
#include <workflow/WFHttpServer.h>

NAMESPACE_BEGIN
class Agent;

struct API ChatbotServerConfig {
  int port = 8888;
  std::string redis_url = "redis://127.0.0.1:6379";
  std::string llm_api_key;
  std::string llm_api_base;
  std::string llm_model = "qwen-plus";
  std::string llm_model_type = "dashscope";
  std::string system_message;
  int max_context = 20;
  std::vector<std::string> tools;
  std::vector<Json> tool_configs;
};

class API ChatbotServer {
public:
  explicit ChatbotServer(const ChatbotServerConfig &cfg);
  ~ChatbotServer();

  bool Start();
  void Stop();

  Agent *getAgent() const { return agent_.get(); }
  const std::string &getRedisUrl() const { return redis_url_; }
  const std::string &getModel() const { return model_; }

private:
  void SetupLLM();
  void Process(WFHttpTask *task);

private:
  ChatbotServerConfig cfg_;
  WFHttpServer server_;
  std::unique_ptr<Agent> agent_{nullptr};
  std::string redis_url_;
  std::string model_;
};

API bool CheckRequest(WFHttpTask *task, const std::string &methold,
                      const std::string &uri);
NAMESPACE_END

#define REGEX_FUNC(func, method, regex, task, ...)                             \
  if (NAMESPACE::CheckRequest(task, method, regex)) {                          \
    func(task, ##__VA_ARGS__);                                                 \
  }
