/*
 * @Author: chenjingyu
 * @Date: 2026-04-18 21:22:36
 * @Contact: 2458006466@qq.com
 * @Description: LLMOpenAI
 */
#include "LLM/LLMOpenAI.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <workflow/Workflow.h>

#include "Base/Logger.h"

using namespace protocol;

NAMESPACE_BEGIN

LLMOpenAI::LLMOpenAI(const Json &cfg) : LLMBase(cfg) {
  if (model_.find("gpt-4o") != std::string::npos ||
      model_.find("vision") != std::string::npos) {
    support_multimodal_ = true;
  }
}

void LLMOpenAI::ChatImpl(const std::vector<Message> &messages,
                         const Json &functions, const GenerateConfig &config,
                         LLMStreamCallback stream_cb,
                         LLMCompleteCallback complete_cb,
                         LLMErrorCallback error_cb) {

  bool is_stream = (stream_cb != nullptr);
  std::string http_body = BuildRequestBody(messages, functions, config, is_stream);

  if (is_stream && stream_cb) {
    WFHttpChunkedTask *chunked_task = CreateStreamTask(
        api_base_ + "/chat/completions", http_body, stream_cb, complete_cb,
        error_cb, new std::vector<Message>(messages));

    SeriesWork *series = Workflow::create_series_work(chunked_task, nullptr);
    series->start();
  } else {
    auto *ctx = new LLMStreamContext();
    ctx->stream_cb = stream_cb;
    ctx->complete_cb = complete_cb;
    ctx->error_cb = error_cb;
    ctx->stream = is_stream;
    ctx->messages = new std::vector<Message>(messages);
    ctx->http_body = http_body;

    WFHttpTask *http_task = WFTaskFactory::create_http_task(
        api_base_ + "/chat/completions", 0, 0, [this, ctx](WFHttpTask *task) {
          this->HttpCallback(task, ctx->stream_cb, ctx->complete_cb,
                             ctx->error_cb, ctx->stream, ctx->messages);
          delete ctx->messages;
          delete ctx;
        });

    HttpRequest *req = http_task->get_req();
    req->set_method("POST");
    req->add_header_pair("Content-Type", "application/json");
    req->add_header_pair("Authorization", "Bearer " + api_key_);
    req->append_output_body_nocopy(ctx->http_body.c_str(), ctx->http_body.size());
    req->set_size_limit(20 * 1024 * 1024);

    SeriesWork *series = Workflow::create_series_work(http_task, nullptr);
    series->set_context(ctx);
    series->start();
  }
}

void LLMOpenAI::HttpCallback(WFHttpTask *task, LLMStreamCallback stream_cb,
                              LLMCompleteCallback complete_cb,
                              LLMErrorCallback error_cb, bool stream,
                              std::vector<Message> *messages) {
  int state = task->get_state();
  int error_code = task->get_error();

  if (state != WFT_STATE_SUCCESS) {
    if (error_cb) {
      error_cb(state, "HTTP request failed: state=" + std::to_string(state) +
                          " error=" + std::to_string(error_code));
    }
    return;
  }

  HttpResponse *resp = task->get_resp();
  const void *body;
  size_t body_len;
  resp->get_parsed_body(&body, &body_len);

  std::string body_str;
  if (body_len > 0) {
    body_str.assign(static_cast<const char *>(body), body_len);
  }

  if (body_str.empty()) {
    if (error_cb) {
      error_cb(-1, "Empty response body from LLM API");
    }
    return;
  }

  size_t input_msg_count = messages->size();

  Json json_resp = Json::parse(body_str);
  if (json_resp.is_valid() && json_resp.has("choices") &&
      json_resp["choices"].size() > 0) {
    Json msg_json = json_resp["choices"][0]["message"];
    std::string content = msg_json.has("content")
                              ? static_cast<std::string>(msg_json["content"])
                              : "";
    Message final_msg(ROLE_ASSISTANT, content);
    messages->push_back(final_msg);
  }
  FormatInfo("After parse: input_count={}, total={}", input_msg_count, messages->size());

  std::vector<Message> assistant_msgs;
  for (size_t i = input_msg_count; i < messages->size(); ++i) {
    if ((*messages)[i].role == ROLE_ASSISTANT) {
      assistant_msgs.push_back((*messages)[i]);
    }
  }

  if (assistant_msgs.empty()) {
    if (body_str.find("\"error\"") != std::string::npos) {
      size_t err_start = body_str.find("{\"error\"");
      if (err_start == std::string::npos)
        err_start = body_str.find("{\\\"error\\\"");
      if (err_start != std::string::npos) {
        Json err_json = Json::parse(body_str.substr(err_start));
        if (err_json.is_valid() && err_json.has("error")) {
          std::string msg = "OpenAI API error: ";
          if (err_json["error"].has("message")) {
            msg += static_cast<std::string>(err_json["error"]["message"]);
          }
          if (err_json["error"].has("code")) {
            msg += " (code: " +
                   static_cast<std::string>(err_json["error"]["code"]) + ")";
          }
          if (error_cb)
            error_cb(-1, msg);
          return;
        }
      }
    }
    std::string dbg =
        "No assistant message. input_count=" + std::to_string(input_msg_count);
    dbg += " total=" + std::to_string(messages->size());
    dbg += " body_len=" + std::to_string(body_str.size());
    dbg += " sample=" + body_str.substr(0, 300);
    if (error_cb) {
      error_cb(-1, dbg);
    }
    return;
  }

  if (complete_cb) {
    complete_cb(assistant_msgs);
  }
}

REGISTERLLM("openai", LLMOpenAI)

NAMESPACE_END
