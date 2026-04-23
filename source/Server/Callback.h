/*
 * @Author: chenjingyu
 * @Date: 2026-04-19 04:00:21
 * @Contact: 2458006466@qq.com
 * @Description: Callback
 */
#pragma once

#include "Api.h"

#include <memory>
#include <string>
#include <workflow/WFTaskFactory.h>

NAMESPACE_BEGIN
class Agent;

struct ChatContext {
  protocol::HttpResponse *resp;
  WFHttpTask *http_task;
  std::string user_id;
  std::string user_prompt;
  bool stream;
  bool header_sent;
  std::string user_msg_json;
  std::string assistant_msg_json;
  std::string redis_url;
  std::string model;
  Agent *agent;
  size_t last_sent_len = 0;
  size_t last_reasoning_sent_len = 0;
};

struct PushChunkData {
  std::string data;
  size_t nleft = 0;
  WFHttpTask *http_task = nullptr;
};

API void HelloWorld(WFHttpTask *task);
API void getHealth(WFHttpTask *task);
API void getModels(WFHttpTask *task);
API void ChatCompletions(WFHttpTask *task, Agent *agent,
                         const std::string &redis_url, const std::string &model);
API void getContext(WFHttpTask *task, const std::string &redis_url);
API void ClearContext(WFHttpTask *task, const std::string &redis_url);

NAMESPACE_END
