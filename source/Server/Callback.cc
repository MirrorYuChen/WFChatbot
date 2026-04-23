/*
 * @Author: chenjingyu
 * @Date: 2026-04-19 04:00:28
 * @Contact: 2458006466@qq.com
 * @Description: Callback
 */
#include "Server/Callback.h"

#include "Agent/Agent.h"
#include "LLM/LLMSchema.h"

#include <cerrno>
#include <sstream>
#include <string>
#include <workflow/HttpMessage.h>
#include <workflow/RedisMessage.h>
#include <workflow/WFFacilities.h>
#include <workflow/WFGlobal.h>
#include <workflow/Workflow.h>

#include "Base/Json.h"
#include "Base/Logger.h"
#include "Tools/ToolBase.h"

using namespace protocol;

NAMESPACE_BEGIN

static std::string MakeRedisKey(const std::string &user_id) {
  return "chat:context:" + user_id;
}

static void SendStreamHeader(protocol::HttpResponse *resp) {
  resp->set_http_version("HTTP/1.1");
  resp->set_status_code("200");
  resp->set_reason_phrase("OK");
  resp->add_header_pair("Content-Type", "text/event-stream");
  resp->add_header_pair("Cache-Control", "no-cache");
  resp->add_header_pair("Connection", "keep-alive");
  resp->add_header_pair("Transfer-Encoding", "chunked");
  resp->add_header_pair("Server", "CodeBlocks ChatBot");
}

static void push_retry_callback(WFTimerTask *timer_task) {
  auto *push_chunk_data = static_cast<PushChunkData *>(timer_task->user_data);
  auto *http_task = push_chunk_data->http_task;
  size_t nleft = push_chunk_data->nleft;
  size_t pos = push_chunk_data->data.size() - nleft;
  size_t nwritten = http_task->push(push_chunk_data->data.c_str() + pos, nleft);
  if (nwritten >= 0) {
    nleft = nleft - nwritten;
  } else {
    nwritten = 0;
    if (errno != EWOULDBLOCK && errno != EAGAIN) {
      delete push_chunk_data;
      return;
    }
  }
  if (nleft > 0) {
    push_chunk_data->nleft = nleft;
    timer_task = WFTaskFactory::create_timer_task(0, 1000000, push_retry_callback);
    timer_task->user_data = push_chunk_data;
    series_of(http_task)->push_front(timer_task);
  } else {
    delete push_chunk_data;
  }
}

static void PushWithRetry(WFHttpTask *task, const std::string &data) {
  size_t nleft = data.size();
  size_t nwritten = task->push(data.c_str(), data.size());
  if (nwritten >= 0) {
    nleft = nleft - nwritten;
  } else {
    nwritten = 0;
    if (errno != EWOULDBLOCK && errno != EAGAIN) {
      return;
    }
  }
  if (nleft > 0) {
    auto *push_chunk_data = new PushChunkData;
    push_chunk_data->data = data;
    push_chunk_data->nleft = nleft;
    push_chunk_data->http_task = task;
    auto *timer_task =
        WFTaskFactory::create_timer_task(0, 1000000, push_retry_callback);
    timer_task->user_data = push_chunk_data;
    series_of(task)->push_front(timer_task);
  }
}

static void SendStreamHeaderPush(WFHttpTask *task) {
  std::string header = "HTTP/1.1 200 OK\r\n";
  header += "Content-Type: text/event-stream\r\n";
  header += "Cache-Control: no-cache\r\n";
  header += "Connection: keep-alive\r\n";
  header += "Transfer-Encoding: chunked\r\n";
  header += "Server: CodeBlocks ChatBot\r\n";
  header += "\r\n";
  PushWithRetry(task, header);
}

static std::string MakeChunkData(const std::string &data) {
  std::stringstream ss;
  ss << std::hex << data.size() << "\r\n";
  ss << data << "\r\n";
  return ss.str();
}

static void SendStreamChunk(protocol::HttpResponse *resp,
                             const std::string &data) {
  if (data.empty())
    return;
  std::string chunked = MakeChunkData(data);
  resp->append_output_body(chunked);
}

static void SendStreamChunkPush(WFHttpTask *task, const std::string &data) {
  if (data.empty())
    return;
  std::string chunked = MakeChunkData(data);
  PushWithRetry(task, chunked);
}

static void SendStreamEnd(protocol::HttpResponse *resp) {
  resp->append_output_body("0\r\n\r\n");
}

static void SendStreamEndPush(WFHttpTask *task) {
  PushWithRetry(task, std::string("0\r\n\r\n", 5));
}

static std::string EscapeJsonString(const std::string &str) {
  std::string result;
  for (char c : str) {
    if (c == '"') {
      result += "\\\"";
    } else if (c == '\\') {
      result += "\\\\";
    } else if (c == '\n') {
      result += "\\n";
    } else if (c == '\r') {
      result += "\\r";
    } else if (c == '\t') {
      result += "\\t";
    } else {
      result += c;
    }
  }
  return result;
}

void HelloWorld(WFHttpTask *task) {
  int state = task->get_state();
  int error = task->get_error();
  if (state == WFT_STATE_TOREPLY) {
    auto *resp = task->get_resp();
    auto seq = task->get_task_seq();
    resp->set_http_version("HTTP/1.1");
    resp->set_status_code("200");
    resp->set_reason_phrase("OK");
    resp->add_header_pair("Content-Type", "text/html");
    resp->add_header_pair("Server", "Server Implemented by Workflow.");

    std::string msg = fmt::format("Hello, World!");
    FormatInfo(msg.c_str());
    resp->append_output_body(msg);
    if (seq == 9) {
      resp->add_header_pair("Connection", "close");
    }
  } else {
    FormatError("Failed HelloWorld with message: {}",
                WFGlobal::get_error_string(state, error));
  }
}

void getHealth(WFHttpTask *task) {
  int state = task->get_state();
  int error = task->get_error();
  if (state == WFT_STATE_TOREPLY) {
    auto *resp = task->get_resp();
    auto seq = task->get_task_seq();
    resp->set_http_version("HTTP/1.1");
    resp->set_status_code("200");
    resp->set_reason_phrase("OK");
    resp->add_header_pair("Content-Type", "application/json");
    resp->add_header_pair("Server", "Server Implemented by Workflow.");

    Json j = Json::Object{};
    j.push_back("status", "ok");
    j.push_back("service", "chatbot");
    auto tools = ToolRegistry::instance().ListTools();
    Json tools_arr = Json::Array{};
    for (const auto &tool_name : tools) {
      tools_arr.push_back(tool_name);
    }
    j["available_tools"] = tools_arr;

    resp->append_output_body_nocopy(j.dump());
    if (seq == 9) {
      resp->add_header_pair("Connection", "close");
    }
  } else {
    FormatError("Failed getHealth with message: {}",
                WFGlobal::get_error_string(state, error));
  }
}

void getModels(WFHttpTask *task) {
  int state = task->get_state();
  int error = task->get_error();
  if (state == WFT_STATE_TOREPLY) {
    auto *resp = task->get_resp();
    auto seq = task->get_task_seq();
    resp->set_http_version("HTTP/1.1");
    resp->set_status_code("200");
    resp->set_reason_phrase("OK");
    resp->add_header_pair("Content-Type", "application/json");
    resp->add_header_pair("Server", "Server Implemented by Workflow.");

    Json j = Json::Object{};
    j["object"] = "list";
    j["data"] = Json::Array{};

    Json model_qwen_turbo = Json::Object{};
    model_qwen_turbo["id"] = "qwen-turbo";
    model_qwen_turbo["object"] = "model";
    model_qwen_turbo["owned_by"] = "alibaba";
    j["data"].push_back(model_qwen_turbo);

    Json model_qwen_plus = Json::Object{};
    model_qwen_plus.push_back("id", "qwen-plus");
    model_qwen_plus.push_back("object", "model");
    model_qwen_plus.push_back("owned_by", "alibaba");
    j["data"].push_back(model_qwen_plus);

    resp->append_output_body_nocopy(j.dump());
    if (seq == 9) {
      resp->add_header_pair("Connection", "close");
    }
  } else {
    FormatError("Failed getModels with message: {}",
                WFGlobal::get_error_string(state, error));
  }
}

void ChatCompletions(WFHttpTask *task, Agent *agent,
                     const std::string &redis_url, const std::string &model) {
  int state = task->get_state();
  int error = task->get_error();
  if (state != WFT_STATE_TOREPLY) {
    FormatError("Failed chatCompletions with message: {}",
                WFGlobal::get_error_string(state, error));
    return;
  }

  auto *req = task->get_req();
  auto *resp = task->get_resp();
  std::string body_str;
  const void *data;
  size_t len;
  req->get_parsed_body(&data, &len);
  body_str.assign((const char *)data, len);

  Json body;
  bool has_valid_input = false;

  if (!body_str.empty()) {
    body = Json::parse(body_str);
    if (body.is_valid()) {
      has_valid_input = true;
    }
  }

  if (!has_valid_input) {
    std::string uri = req->get_request_uri();
    size_t qpos = uri.find('?');
    if (qpos != std::string::npos) {
      std::string query = uri.substr(qpos + 1);
      std::string user_id_param, message_param;
      size_t pos = 0;
      while (pos < query.size()) {
        size_t amp = query.find('&', pos);
        std::string pair = query.substr(pos, amp - pos);
        size_t eq = pair.find('=');
        if (eq != std::string::npos) {
          std::string key = pair.substr(0, eq);
          std::string val = pair.substr(eq + 1);
          if (key == "user_id") {
            user_id_param = val;
          } else if (key == "message") {
            message_param = val;
          }
        }
        if (amp == std::string::npos)
          break;
        pos = amp + 1;
      }
      if (!user_id_param.empty() && !message_param.empty()) {
        body = Json::Object{};
        body.push_back("user_id", user_id_param);
        Json msgs = Json::Array{};
        Json user_msg = Json::Object{};
        user_msg.push_back("role", "user");
        user_msg.push_back("content", message_param);
        msgs.push_back(user_msg);
        body.push_back("messages", msgs);
        has_valid_input = true;
      }
    }
  }

  if (!has_valid_input) {
    Json j = Json::Object{};
    j["error"]["message"] =
        "empty request body or invalid format. Use body with user_id and "
        "messages, or query params: ?user_id=xxx&message=xxx";
    j["error"]["type"] = "invalid_request_error";
    resp->add_header_pair("Content-Type", "application/json");
    resp->append_output_body(j.dump());
    return;
  }

  std::string user_id;
  if (body.has("user_id")) {
    user_id = static_cast<std::string>(body["user_id"]);
  }

  std::string prompt;
  if (body.has("messages") && body["messages"].is_array() &&
      body["messages"].size() > 0) {
    const Json &last_msg = body["messages"][body["messages"].size() - 1];
    if (last_msg.has("content")) {
      prompt = static_cast<std::string>(last_msg["content"]);
    }
  }

  bool stream = false;
  if (body.has("stream")) {
    stream = static_cast<bool>(body["stream"]);
  }

  if (prompt.empty() || user_id.empty()) {
    Json j = Json::Object{};
    j["error"]["message"] = "messages and user_id are required";
    j["error"]["type"] = "invalid_request_error";
    resp->add_header_pair("Content-Type", "application/json");
    resp->append_output_body(j.dump());
    return;
  }

  auto *ctx = new ChatContext;
  ctx->resp = resp;
  ctx->http_task = task;
  ctx->user_id = user_id;
  ctx->user_prompt = prompt;
  ctx->stream = stream;
  ctx->header_sent = false;
  ctx->redis_url = redis_url;
  ctx->model = model;
  ctx->agent = agent;

  std::string redis_key = MakeRedisKey(user_id);
  std::string local_redis_url = redis_url;

  WFRedisTask *redis_task = WFTaskFactory::create_redis_task(
      local_redis_url, 0,
      [ctx, redis_key, local_redis_url](WFRedisTask *redis_task) {
        int state = redis_task->get_state();
        std::vector<Message> messages;

        if (state == WFT_STATE_SUCCESS) {
          protocol::RedisValue val;
          redis_task->get_resp()->get_result(val);
          if (val.is_array()) {
            for (size_t i = 0; i < val.arr_size(); i++) {
              const protocol::RedisValue &elem = val.arr_at(i);
              if (elem.is_string()) {
                Json msg_json = Json::parse(elem.string_value());
                if (msg_json.is_valid()) {
                  Message msg;
                  if (msg_json.has("role") && msg_json["role"].is_string())
                    msg.role = msg_json["role"].get<std::string>();
                  if (msg_json.has("content") &&
                      msg_json["content"].is_string())
                    msg.content = msg_json["content"].get<std::string>();
                  messages.push_back(msg);
                }
              }
            }
          }
        }

        Message user_msg;
        user_msg.role = ROLE_USER;
        user_msg.content = ctx->user_prompt;
        ctx->user_msg_json =
            "{\"role\":\"user\",\"content\":\"" + user_msg.content + "\"}";
        messages.push_back(user_msg);

        if (ctx->stream) {
          auto *wg = new WFFacilities::WaitGroup(1);

          SendStreamHeaderPush(ctx->http_task);
          ctx->header_sent = true;

          ctx->agent->Run(
              messages,
              [ctx](const std::vector<Message> &chunk) {
                if (!chunk.empty()) {
                  const Message &msg = chunk.back();
                  Json delta_obj = Json::Object{};
                  bool has_delta = false;

                  // Detect reset (new turn)
                  if (msg.content.size() < ctx->last_sent_len) {
                    ctx->last_sent_len = 0;
                  }
                  if (msg.reasoning_content.size() <
                      ctx->last_reasoning_sent_len) {
                    ctx->last_reasoning_sent_len = 0;
                  }

                  // Handle content delta
                  if (msg.content.size() > ctx->last_sent_len) {
                    delta_obj["content"] =
                        msg.content.substr(ctx->last_sent_len);
                    ctx->last_sent_len = msg.content.size();
                    has_delta = true;
                  }

                  // Handle reasoning_content delta
                  if (msg.reasoning_content.size() >
                      ctx->last_reasoning_sent_len) {
                    delta_obj["reasoning_content"] = msg.reasoning_content.substr(
                        ctx->last_reasoning_sent_len);
                    ctx->last_reasoning_sent_len =
                        msg.reasoning_content.size();
                    has_delta = true;
                  }

                  if (has_delta) {
                    Json chunk_json = Json::Object{};
                    chunk_json["id"] = "chatcmpl-agent";
                    chunk_json["object"] = "chat.completion.chunk";
                    chunk_json["model"] = ctx->model;
                    chunk_json["choices"] =
                        Json::Array{Json::Object{{"delta", delta_obj},
                                                 {"index", 0},
                                                 {"finish_reason", nullptr}}};
                    std::string data = "data: " + chunk_json.dump() + "\n\n";
                    SendStreamChunkPush(ctx->http_task, data);
                  }
                }
              },
              [ctx, wg, local_redis_url](const std::vector<Message> &response) {
                SendStreamChunkPush(ctx->http_task, std::string("data: [DONE]\n\n"));
                SendStreamEndPush(ctx->http_task);

                if (!response.empty()) {
                  std::string content = response.back().content;
                  Json assistant_msg = Json::Object{};
                  assistant_msg["role"] = "assistant";
                  assistant_msg["content"] = content;
                  ctx->assistant_msg_json = assistant_msg.dump();

                  std::string redis_key = MakeRedisKey(ctx->user_id);

                  WFRedisTask *redis_rpush1 = WFTaskFactory::create_redis_task(
                      local_redis_url, 0, nullptr);
                  redis_rpush1->get_req()->set_request(
                      "RPUSH", {redis_key, ctx->user_msg_json});

                  WFRedisTask *redis_rpush2 = WFTaskFactory::create_redis_task(
                      local_redis_url, 0, nullptr);
                  redis_rpush2->get_req()->set_request(
                      "RPUSH", {redis_key, ctx->assistant_msg_json});

                  WFRedisTask *redis_ltrim = WFTaskFactory::create_redis_task(
                      local_redis_url, 0, nullptr);
                  redis_ltrim->get_req()->set_request("LTRIM",
                                                      {redis_key, "-20", "-1"});

                  SeriesWork *series = series_of(ctx->http_task);
                  series->push_back(redis_rpush1);
                  series->push_back(redis_rpush2);
                  series->push_back(redis_ltrim);
                }

                delete ctx;
                wg->done();
              },
              [ctx, wg](int code, const std::string &error) {
                Json j;
                j["error"]["message"] = error;
                j["error"]["type"] = "api_error";
                j["error"]["code"] = code;
                std::string err_str = j.dump();
                SendStreamChunkPush(ctx->http_task,
                                std::string("data: " + err_str + "\n\n"));
                SendStreamChunkPush(ctx->http_task, std::string("data: [DONE]\n\n"));
                SendStreamEndPush(ctx->http_task);
                delete ctx;
                wg->done();
              });

          series_of(ctx->http_task)
              ->push_back(WFTaskFactory::create_go_task("agent_wait", [wg]() {
                wg->wait();
                delete wg;
              }));
        } else {
          std::vector<Message> result = ctx->agent->RunSync(messages);

          if (!result.empty()) {
            std::string content = result.back().content;

            Json resp_json = Json::Object{};
            resp_json["choices"] = Json::Array{
                Json::Object{{"message", Json::Object{{"role", "assistant"},
                                                      {"content", content}}},
                             {"finish_reason", "stop"}}};
            resp_json["model"] = ctx->model;

            ctx->resp->add_header_pair("Content-Type", "application/json");
            ctx->resp->append_output_body(resp_json.dump());

            Json assistant_msg = Json::Object{};
            assistant_msg["role"] = "assistant";
            assistant_msg["content"] = content;
            ctx->assistant_msg_json = assistant_msg.dump();

            std::string redis_key = MakeRedisKey(ctx->user_id);
            WFRedisTask *redis_rpush1 =
                WFTaskFactory::create_redis_task(local_redis_url, 0, nullptr);
            redis_rpush1->get_req()->set_request(
                "RPUSH", {redis_key, ctx->user_msg_json});

            WFRedisTask *redis_rpush2 =
                WFTaskFactory::create_redis_task(local_redis_url, 0, nullptr);
            redis_rpush2->get_req()->set_request(
                "RPUSH", {redis_key, ctx->assistant_msg_json});

            WFRedisTask *redis_ltrim =
                WFTaskFactory::create_redis_task(local_redis_url, 0, nullptr);
            redis_ltrim->get_req()->set_request("LTRIM",
                                                {redis_key, "-20", "-1"});

            SeriesWork *series = series_of(ctx->http_task);
            series->push_back(redis_rpush1);
            series->push_back(redis_rpush2);
            series->push_back(redis_ltrim);
          } else {
            Json j;
            j["error"]["message"] = "LLM returned empty response";
            j["error"]["type"] = "api_error";
            ctx->resp->add_header_pair("Content-Type", "application/json");
            ctx->resp->append_output_body(j.dump());
          }
          delete ctx;
        }
      });

  redis_task->get_req()->set_request("LRANGE", {redis_key, "0", "-1"});
  redis_task->user_data = ctx;

  series_of(task)->push_back(redis_task);
}

void getContext(WFHttpTask *task, const std::string &redis_url) {
  int state = task->get_state();
  int error = task->get_error();
  if (state != WFT_STATE_TOREPLY) {
    FormatError("Failed getContext with message: {}",
                WFGlobal::get_error_string(state, error));
    return;
  }

  auto *req = task->get_req();
  auto *resp = task->get_resp();
  std::string uri = req->get_request_uri();

  size_t pos = uri.find("/v1/context/");
  if (pos == std::string::npos) {
    Json j = Json::Object{};
    j["error"]["message"] = "user_id is required";
    j["error"]["type"] = "invalid_request_error";
    resp->add_header_pair("Content-Type", "application/json");
    resp->append_output_body(j.dump());
    return;
  }

  std::string user_id = uri.substr(pos + 12);
  if (user_id.empty()) {
    Json j = Json::Object{};
    j["error"]["message"] = "user_id is required";
    j["error"]["type"] = "invalid_request_error";
    resp->add_header_pair("Content-Type", "application/json");
    resp->append_output_body(j.dump());
    return;
  }

  std::string redis_key = MakeRedisKey(user_id);
  WFRedisTask *redis_task = WFTaskFactory::create_redis_task(
      redis_url, 0, [resp, user_id](WFRedisTask *task) {
        Json j = Json::Object{};
        j["user_id"] = user_id;

        if (task->get_state() == WFT_STATE_SUCCESS) {
          protocol::RedisValue val;
          task->get_resp()->get_result(val);

          j["messages"] = Json::Array{};
          if (val.is_array()) {
            for (size_t i = 0; i < val.arr_size(); i++) {
              const protocol::RedisValue &elem = val.arr_at(i);
              if (elem.is_string()) {
                Json msg = Json::parse(elem.string_value());
                if (msg.is_valid()) {
                  j["messages"].push_back(msg);
                }
              }
            }
          }
          j["count"] = static_cast<int>(val.arr_size());
        } else {
          j["error"]["message"] = "failed to get context";
          j["error"]["type"] = "redis_error";
        }
        resp->add_header_pair("Content-Type", "application/json");
        resp->append_output_body(j.dump());
      });
  redis_task->get_req()->set_request("LRANGE", {redis_key, "0", "-1"});
  series_of(task)->push_back(redis_task);
}

void ClearContext(WFHttpTask *task, const std::string &redis_url) {
  int state = task->get_state();
  int error = task->get_error();
  if (state != WFT_STATE_TOREPLY) {
    FormatError("Failed clearContext with message: {}",
                WFGlobal::get_error_string(state, error));
    return;
  }

  auto *req = task->get_req();
  auto *resp = task->get_resp();
  std::string uri = req->get_request_uri();

  size_t pos = uri.find("/v1/context/");
  if (pos == std::string::npos) {
    Json j = Json::Object{};
    j["error"]["message"] = "user_id is required";
    j["error"]["type"] = "invalid_request_error";
    resp->add_header_pair("Content-Type", "application/json");
    resp->append_output_body(j.dump());
    return;
  }

  std::string user_id = uri.substr(pos + 12);
  if (user_id.empty()) {
    Json j = Json::Object{};
    j["error"]["message"] = "user_id is required";
    j["error"]["type"] = "invalid_request_error";
    resp->add_header_pair("Content-Type", "application/json");
    resp->append_output_body(j.dump());
    return;
  }

  std::string redis_key = MakeRedisKey(user_id);
  WFRedisTask *redis_task =
      WFTaskFactory::create_redis_task(redis_url, 0, [resp](WFRedisTask *task) {
        Json j = Json::Object{};
        if (task->get_state() == WFT_STATE_SUCCESS) {
          j["status"] = "success";
          j["message"] = "context cleared for user";
        } else {
          j["error"]["message"] = "failed to clear context";
          j["error"]["type"] = "redis_error";
        }
        resp->add_header_pair("Content-Type", "application/json");
        resp->append_output_body(j.dump());
      });
  redis_task->get_req()->set_request("DEL", {redis_key});
  series_of(task)->push_back(redis_task);
}
NAMESPACE_END
