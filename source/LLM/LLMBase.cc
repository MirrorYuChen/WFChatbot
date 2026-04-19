/*
 * @Author: chenjingyu
 * @Date: 2026-04-18 19:17:00
 * @Contact: 2458006466@qq.com
 * @Description: LLMBase
 */
#include "LLM/LLMBase.h"
#include "Base/Json.h"
#include "Base/Logger.h"

#include <algorithm>
#include <cstdlib>
#include <sstream>
#include <workflow/HttpMessage.h>
#include <workflow/WFFacilities.h>

NAMESPACE_BEGIN
LLMBase::LLMBase(const Json &cfg) {
  if (cfg.has("model")) {
    model_ = static_cast<std::string>(cfg["model"]);
  }
  if (cfg.has("model_type")) {
    model_type_ = static_cast<std::string>(cfg["model_type"]);
  }
  FormatInfo("model_type: {}, has api_key in cfg: {}", model_type_, cfg.has("api_key") ? "true" : "false");
  if (cfg.has("api_key")) {
    api_key_ = static_cast<std::string>(cfg["api_key"]);
    std::cout << "LLMBase has api key: " << api_key_ << "\n";
    FormatInfo("api_key from cfg: {}...", api_key_);
  } else {
    std::cout << "LLMBase does not has api key\n";
    const char *env_key = nullptr;
    if (model_type_ == "deepseek") {
      env_key = std::getenv("DEEPSEEK_API_KEY");
    } else if (model_type_ == "dashscope" || model_type_ == "qwen_dashscope") {
      env_key = std::getenv("DASHSCOPE_API_KEY");
    } else if (model_type_ == "openai" || model_type_ == "openai_openai") {
      env_key = std::getenv("OPENAI_API_KEY");
    }
    if (env_key) {
      api_key_ = env_key;
    }
  }
  if (cfg.has("api_base")) {
    api_base_ = static_cast<std::string>(cfg["api_base"]);
  } else {
    if (model_type_ == "deepseek") {
      api_base_ = "https://api.deepseek.com/v1";
    } else if (model_type_ == "openai" || model_type_ == "openai_openai") {
      api_base_ = "https://api.openai.com/v1";
    } else {
      api_base_ = "https://dashscope.aliyuncs.com/compatible-mode/v1";
    }
  }
  if (cfg.has("generate_cfg")) {
    const Json &gen_cfg = cfg["generate_cfg"];
    if (gen_cfg.has("temperature")) {
      generate_cfg_.temperature = gen_cfg["temperature"].get<double>();
    }
    if (gen_cfg.has("top_p")) {
      generate_cfg_.top_p = gen_cfg["top_p"].get<double>();
    }
    if (gen_cfg.has("max_tokens")) {
      generate_cfg_.max_tokens = gen_cfg["max_tokens"].get<int>();
    }
    if (gen_cfg.has("max_input_tokens")) {
      generate_cfg_.max_input_tokens = gen_cfg["max_input_tokens"].get<int>();
    }
    if (gen_cfg.has("max_retries")) {
      max_retries_ = gen_cfg["max_retries"].get<int>();
    }
    if (gen_cfg.has("enable_thinking")) {
      generate_cfg_.enable_thinking =
          static_cast<bool>(gen_cfg["enable_thinking"]);
    }
    if (gen_cfg.has("use_raw_api")) {
      generate_cfg_.use_raw_api = static_cast<bool>(gen_cfg["use_raw_api"]);
    }
  }
}

std::vector<Message>
LLMBase::ChatSync(const std::vector<Message> &messages,
                  const std::vector<FunctionDef> &functions,
                  const GenerateConfig &config) {

  std::vector<Message> result;
  WFFacilities::WaitGroup wg(1);

  Json functions_json = Json::Array{};
  for (const auto &func : functions) {
    functions_json.push_back(func.ToJson());
  }

  ChatImpl(
      messages, functions_json, config,
      nullptr, // No streaming callback
      [&result, &wg](const std::vector<Message> &msgs) {
        result = msgs;
        wg.done();
      },
      [this, &wg](int code, const std::string &msg) {
        // Error handling - could throw or log
        FormatError("{} LLM error: {} - {}", model_, code, msg); 
        wg.done();
      });

  wg.wait();
  return result;
}

void LLMBase::ChatStream(const std::vector<Message> &messages,
                         const std::vector<FunctionDef> &functions,
                         const GenerateConfig &config,
                         LLMStreamCallback stream_cb,
                         LLMCompleteCallback complete_cb,
                         LLMErrorCallback error_cb) {

  Json functions_json = Json::Array{};
  for (const auto &func : functions) {
    functions_json.push_back(func.ToJson());
  }

  ChatImpl(messages, functions_json, config, stream_cb, complete_cb, error_cb);
}

void LLMBase::ChatStream(const std::vector<Message> &messages,
                         LLMStreamCallback stream_cb,
                         LLMCompleteCallback complete_cb,
                         LLMErrorCallback error_cb) {

  ChatImpl(messages, Json::Array{}, generate_cfg_, stream_cb, complete_cb,
           error_cb);
}

LLMResponse LLMBase::ParseStreamChunk(const std::string &chunk) {
  LLMResponse resp;
  Json j = Json::parse(chunk);
  if (j.is_valid() && j.has("choices") && j["choices"].size() > 0) {
    Json delta = j["choices"][0]["delta"];
    if (delta.has("content")) {
      resp.content = static_cast<std::string>(delta["content"]);
    }
    if (delta.has("reasoning_content")) {
      resp.reasoning_content =
          static_cast<std::string>(delta["reasoning_content"]);
    }
    if (j.has("choices") && j["choices"][0].has("finish_reason")) {
      std::string reason =
          static_cast<std::string>(j["choices"][0]["finish_reason"]);
      if (reason != "null" && !reason.empty()) {
        resp.is_finished = true;
        resp.finish_reason = reason;
      }
    }
  }
  return resp;
}

LLMResponse LLMBase::ParseFullResponse(const Json &json_resp) {
  LLMResponse resp;
  if (json_resp.has("choices") && json_resp["choices"].size() > 0) {
    Json message = json_resp["choices"][0]["message"];
    if (message.has("content")) {
      resp.content = static_cast<std::string>(message["content"]);
    }
    if (message.has("reasoning_content")) {
      resp.reasoning_content =
          static_cast<std::string>(message["reasoning_content"]);
    }
    if (message.has("function_call")) {
      resp.function_call = FunctionCall::FromJson(message["function_call"]);
    }
    if (message.has("tool_calls") && message["tool_calls"].is_array()) {
      for (size_t i = 0; i < message["tool_calls"].size(); ++i) {
        resp.tool_calls.push_back(ToolCall::FromJson(message["tool_calls"][i]));
      }
    }
    if (json_resp["choices"][0].has("finish_reason")) {
      resp.finish_reason =
          static_cast<std::string>(json_resp["choices"][0]["finish_reason"]);
      resp.is_finished = true;
    }
  }
  if (json_resp.has("usage")) {
    resp.usage = json_resp["usage"].copy();
  }
  return resp;
}

std::string LLMBase::BuildRequestBody(const std::vector<Message> &messages,
                                      const Json &functions,
                                      const GenerateConfig &config,
                                      bool stream) {

  // Build messages array
  Json msg_array = Json::Array{};
  for (const auto &msg : messages) {
    std::string role = msg.role;
    if (role.empty())
      role = ROLE_USER;
    if (role == ROLE_TOOL)
      role = ROLE_FUNCTION;

    Json msg_json = Json::Object{};
    msg_json["role"] = role;

    if (role == ROLE_ASSISTANT) {
      msg_json["content"] = msg.content.empty() ? "" : msg.content;
      if (!msg.reasoning_content.empty())
        msg_json["reasoning_content"] = msg.reasoning_content;
      if (msg.HasFunctionCall())
        msg_json["function_call"] = msg.function_call.ToJson();
      if (msg.HasToolCalls()) {
        Json tc_array = Json::Array{};
        for (const auto &tc : msg.tool_calls)
          tc_array.push_back(tc.ToJson());
        msg_json["tool_calls"] = tc_array;
      }
    } else if (role == ROLE_FUNCTION) {
      msg_json["content"] = msg.content;
      if (!msg.name.empty())
        msg_json["name"] = msg.name;
    } else {
      msg_json["content"] = msg.content;
      if (!msg.name.empty())
        msg_json["name"] = msg.name;
    }
    msg_array.push_back(msg_json);
  }

  // Use snprintf with %g to produce locale-independent, compact float
  // representation
  char temp_buf[64];
  snprintf(temp_buf, sizeof(temp_buf), "%g", config.temperature);
  char top_p_buf[64];
  snprintf(top_p_buf, sizeof(top_p_buf), "%g", config.top_p);

  std::string body =
      "{\"model\":\"" + model_ + "\",\"messages\":" + msg_array.dump();
  body += ",\"stream\":" + std::string(stream ? "true" : "false");
  body += ",\"temperature\":";
  body += temp_buf;
  body += ",\"top_p\":";
  body += top_p_buf;
  if (config.max_tokens > 0) {
    body += ",\"max_tokens\":" + std::to_string(config.max_tokens);
  }
  body += "}";

  return body;
}

Json LLMBase::ParseResponseBody(const std::string &body) {
  return Json::parse(body);
}

std::vector<Message>
LLMBase::TruncateMessages(const std::vector<Message> &messages,
                          int max_tokens) {
  // Simple truncation - could be more sophisticated
  std::vector<Message> result;
  int total_tokens = 0;

  // Keep system messages
  for (const auto &msg : messages) {
    if (msg.role == ROLE_SYSTEM) {
      result.push_back(msg);
      total_tokens += EstimateTokens(msg.content);
    }
  }

  // Add other messages until we hit the limit
  for (const auto &msg : messages) {
    if (msg.role == ROLE_SYSTEM)
      continue;
    int msg_tokens = EstimateTokens(msg.content);
    if (total_tokens + msg_tokens > max_tokens) {
      break;
    }
    result.push_back(msg);
    total_tokens += msg_tokens;
  }

  return result;
}

int LLMBase::EstimateTokens(const std::string &text) {
  // Rough estimate: ~1 token per 1.3 characters for English
  // This is a simplification - real tokenization depends on the model
  return static_cast<int>(text.size() / 1.3);
}

// LLM Registry implementation
LLMRegistry &LLMRegistry::instance() {
  static LLMRegistry registry;
  return registry;
}

void LLMRegistry::RegisterLLM(const std::string &name, LLMCreator creator) {
  creators_[name] = creator;
}

LLMBase *LLMRegistry::CreateLLM(const std::string &name, const Json &cfg) {
  auto it = creators_.find(name);
  if (it != creators_.end()) {
    return it->second(cfg);
  }
  return nullptr;
}

void LLMBase::InitStreamContext(LLMStreamContext *ctx,
                                 const std::vector<Message> &messages,
                                 const std::string &http_body,
                                 LLMStreamCallback stream_cb,
                                 LLMCompleteCallback complete_cb,
                                 LLMErrorCallback error_cb) {
  ctx->stream_cb = stream_cb;
  ctx->complete_cb = complete_cb;
  ctx->error_cb = error_cb;
  ctx->stream = true;
  ctx->messages = new std::vector<Message>(messages);
  ctx->http_body = http_body;
  ctx->accumulated_content = "";
  ctx->reasoning_content = "";
  ctx->sse_buffer = "";
}

WFHttpChunkedTask *LLMBase::CreateStreamTask(
    const std::string &url, const std::string &http_body,
    LLMStreamCallback stream_cb, LLMCompleteCallback complete_cb,
    LLMErrorCallback error_cb, std::vector<Message> *messages) {

  auto *ctx = new LLMStreamContext();
  InitStreamContext(ctx, *messages, http_body, stream_cb, complete_cb, error_cb);

  WFHttpChunkedTask *chunked_task = WFHttpChunkedClient::create_chunked_task(
      url, 0,
      [ctx](WFHttpChunkedTask *task) {},
      [this, ctx](WFHttpChunkedTask *task) {
        this->HandleStreamComplete(task, ctx);
        delete ctx->messages;
        delete ctx;
      });

  chunked_task->set_extract([this, ctx](WFHttpChunkedTask *task) {
    const protocol::HttpMessageChunk *chunk = task->get_chunk();
    if (chunk) {
      const void *data;
      size_t size;
      if (chunk->get_chunk_data(&data, &size) && data && size > 0) {
        this->ProcessSseChunk(data, size, ctx);
      }
    }
  });

  protocol::HttpRequest *req = chunked_task->get_req();
  req->set_method("POST");
  req->add_header_pair("Content-Type", "application/json");
  req->add_header_pair("Authorization", "Bearer " + api_key_);
  req->append_output_body_nocopy(ctx->http_body.c_str(), ctx->http_body.size());
  req->set_size_limit(20 * 1024 * 1024);

  return chunked_task;
}

void LLMBase::ProcessSseChunk(const void *data, size_t size,
                              LLMStreamContext *ctx) {
  std::string chunk_data(static_cast<const char *>(data), size);
  ctx->sse_buffer += chunk_data;

  size_t pos = 0;
  while (true) {
    size_t line_end = ctx->sse_buffer.find('\n', pos);
    if (line_end == std::string::npos) {
      ctx->sse_buffer = ctx->sse_buffer.substr(pos);
      break;
    }

    std::string line = ctx->sse_buffer.substr(pos, line_end - pos);
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    pos = line_end + 1;

    if (line.empty())
      continue;

    if (line.rfind("data: ", 0) == 0) {
      std::string json_str = line.substr(6);
      if (json_str == "[DONE]") {
        ctx->sse_buffer.clear();
        break;
      }

      Json chunk_json = Json::parse(json_str);
      if (!chunk_json.is_valid() || chunk_json.has("error")) {
        continue;
      }
      if (!chunk_json.has("choices") || chunk_json["choices"].size() == 0) {
        continue;
      }

      Json delta = chunk_json["choices"][0]["delta"];
      std::string delta_content;

      if (delta.has("reasoning_content")) {
        ctx->reasoning_content +=
            static_cast<std::string>(delta["reasoning_content"]);
      }

      if (delta.has("content")) {
        delta_content = static_cast<std::string>(delta["content"]);
        ctx->accumulated_content += delta_content;
      }

      Message msg(ROLE_ASSISTANT, delta_content);
      if (!ctx->reasoning_content.empty()) {
        msg.reasoning_content = ctx->reasoning_content;
      }

      if (ctx->stream_cb && !delta_content.empty()) {
        ctx->stream_cb({msg});
      }
    }
  }
}

void LLMBase::HandleStreamComplete(WFHttpChunkedTask *task,
                                   LLMStreamContext *ctx) {
  int state = task->get_state();
  if (state != WFT_STATE_SUCCESS) {
    if (ctx->error_cb) {
      ctx->error_cb(state, "HTTP request failed in streaming");
    }
    return;
  }

  if (!ctx->stream_cb || !ctx->complete_cb)
    return;

  Message final_msg(ROLE_ASSISTANT, ctx->accumulated_content);
  if (!ctx->reasoning_content.empty()) {
    final_msg.reasoning_content = ctx->reasoning_content;
  }
  ctx->messages->push_back(final_msg);

  std::vector<Message> assistant_msgs;
  for (size_t i = 0; i < ctx->messages->size(); ++i) {
    if ((*ctx->messages)[i].role == ROLE_ASSISTANT) {
      assistant_msgs.push_back((*ctx->messages)[i]);
    }
  }

  if (ctx->complete_cb) {
    ctx->complete_cb(assistant_msgs);
  }
}

NAMESPACE_END
