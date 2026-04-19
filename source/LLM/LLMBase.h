/*
 * @Author: chenjingyu
 * @Date: 2026-04-18 19:16:52
 * @Contact: 2458006466@qq.com
 * @Description: LLMBase
 */
#pragma once

#include "Api.h"
#include "Base/Json.h"
#include "LLM/LLMSchema.h"
#include <string>
#include <vector>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFHttpChunkedClient.h>

NAMESPACE_BEGIN
// LLM generation configuration
struct API GenerateConfig {
  double temperature = 0.7;
  double top_p = 0.8;
  int max_tokens = 0;
  int max_input_tokens = 50000;
  int seed = -1;
  bool enable_thinking = false;
  std::string fncall_promt_type = "nous";
  bool thought_in_content = false;
  bool use_raw_api = false;
  int max_retries = 3;
  std::vector<std::string> stop;
};

// LLM response chunk for streaming
struct API LLMResponse {
  std::string content;
  std::string reasoning_content;
  FunctionCall function_call;
  std::vector<ToolCall> tool_calls;
  bool is_finished = false;
  std::string finish_reason;
  Json usage; // token usage info
};

// Forward declaration
class LLMBase;

// Callback type for async LLM calls
using LLMStreamCallback = std::function<void(const std::vector<Message> &)>;
using LLMCompleteCallback = std::function<void(const std::vector<Message> &)>;
using LLMErrorCallback = std::function<void(int, const std::string &)>;

// LLM registry type
using LLMCreator = std::function<LLMBase *(const Json &)>;

// Stream context for SSE streaming
struct LLMStreamContext {
  LLMStreamCallback stream_cb;
  LLMCompleteCallback complete_cb;
  LLMErrorCallback error_cb;
  bool stream;
  std::vector<Message> *messages;
  std::string http_body;
  std::string accumulated_content;
  std::string reasoning_content;
  std::string sse_buffer;
};

class API LLMBase {
public:
  LLMBase(const Json &cfg);
  virtual ~LLMBase() = default;

  // Synchronous chat (blocking)
  std::vector<Message> ChatSync(const std::vector<Message> &messages,
                                const std::vector<FunctionDef> &functions = {},
                                const GenerateConfig &config = {});

  // Asynchronous chat with streaming callback
  void ChatStream(const std::vector<Message> &messages,
                  const std::vector<FunctionDef> &functions,
                  const GenerateConfig &config, LLMStreamCallback stream_cb,
                  LLMCompleteCallback complete_cb, LLMErrorCallback error_cb);

  // Asynchronous chat with streaming (simplified)
  void ChatStream(const std::vector<Message> &messages,
                  LLMStreamCallback stream_cb, LLMCompleteCallback complete_cb,
                  LLMErrorCallback error_cb);

  // Get model name
  const std::string &model() const { return model_; }
  const std::string &model_type() const { return model_type_; }

  // Check capabilities
  virtual bool support_multimodal_input() const { return false; }
  virtual bool support_multimodal_output() const { return false; }

protected:
  // Pure virtual methods for subclasses
  virtual void ChatImpl(const std::vector<Message> &messages,
                        const Json &functions, const GenerateConfig &config,
                        LLMStreamCallback stream_cb,
                        LLMCompleteCallback complete_cb,
                        LLMErrorCallback error_cb) = 0;

  // Parse streaming response
  virtual LLMResponse ParseStreamChunk(const std::string &chunk);
  virtual LLMResponse ParseFullResponse(const Json &json_resp);

  // Build HTTP request body
  virtual std::string BuildRequestBody(const std::vector<Message> &messages,
                                       const Json &functions,
                                       const GenerateConfig &config,
                                       bool stream = true);

  // Parse HTTP response
  virtual Json ParseResponseBody(const std::string &body);

  // Truncate messages if too long
  std::vector<Message> TruncateMessages(const std::vector<Message> &messages,
                                        int max_tokens);

  // Estimate token count (rough)
  int EstimateTokens(const std::string &text);

  // Create streaming HTTP task with chunked callback
  WFHttpChunkedTask *CreateStreamTask(
      const std::string &url, const std::string &http_body,
      LLMStreamCallback stream_cb, LLMCompleteCallback complete_cb,
      LLMErrorCallback error_cb, std::vector<Message> *messages);

  // Process SSE chunk data (to be called from extract callback)
  void ProcessSseChunk(const void *data, size_t size, LLMStreamContext *ctx);

  // Handle stream completion
  void HandleStreamComplete(WFHttpChunkedTask *task, LLMStreamContext *ctx);

  // Initialize stream context
  void InitStreamContext(LLMStreamContext *ctx, const std::vector<Message> &messages,
                          const std::string &http_body, LLMStreamCallback stream_cb,
                          LLMCompleteCallback complete_cb, LLMErrorCallback error_cb);

protected:
  std::string model_;
  std::string model_type_;
  std::string api_key_;
  std::string api_base_;
  GenerateConfig generate_cfg_;
  int max_retries_ = 3;
};

// Registry for LLM types
class LLMRegistry {
public:
  static LLMRegistry &instance();

  void RegisterLLM(const std::string &name, LLMCreator creator);
  LLMBase *CreateLLM(const std::string &name, const Json &cfg);

private:
  std::map<std::string, LLMCreator> creators_;
};

// Helper macro to register LLM types
#define REGISTERLLM(name, cls)                                                \
  static bool __register##cls = []() {                                        \
    LLMRegistry::instance().RegisterLLM(                                      \
        name, [](const Json &cfg) -> LLMBase * { return new cls(cfg); });      \
    return true;                                                               \
  }();

NAMESPACE_END
