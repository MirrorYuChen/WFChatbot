/*
 * @Author: chenjingyu
 * @Date: 2026-04-18 23:21:17
 * @Contact: 2458006466@qq.com
 * @Description: Agent
 */
#include "Agent/Agent.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <workflow/WFFacilities.h>

NAMESPACE_BEGIN
Agent::Agent(Config &&config)
    : system_message_(std::move(config.system_message)),
      name_(std::move(config.name)),
      description_(std::move(config.description)),
      generate_cfg_(config.generate_cfg) {

  // Initialize LLM: prefer direct instance, fall back to registry
  if (config.llm) {
    llm_ = std::move(config.llm);
  } else if (config.llm_cfg.is_valid() && !config.llm_cfg.is_null()) {
    std::string model_type = "dashscope";
    if (config.llm_cfg.has("model_type")) {
      model_type = static_cast<std::string>(config.llm_cfg["model_type"]);
    }
    llm_.reset(LLMRegistry::instance().CreateLLM(model_type, config.llm_cfg));
  }

  // Initialize tools from names
  for (const auto &tool_name : config.tools) {
    Json tool_cfg;
    if (!config.tool_configs.empty()) {
      for (const auto &tc : config.tool_configs) {
        if (tc.has("name") &&
            static_cast<std::string>(tc["name"]) == tool_name) {
          tool_cfg = tc;
          break;
        }
      }
    }
    ToolBase *tool = ToolRegistry::instance().CreateTool(tool_name, tool_cfg);
    if (tool) {
      tools_[tool->name()] = std::unique_ptr<ToolBase>(tool);
    }
  }

  // Add custom tool instances
  for (auto *tool : config.custom_tools) {
    tools_[tool->name()] = std::unique_ptr<ToolBase>(tool);
  }
}

void Agent::Run(const std::vector<Message> &messages,
                AgentStreamCallback stream_cb,
                AgentCompleteCallback complete_cb,
                AgentErrorCallback error_cb) {

  std::string lang = DetectLanguage(messages);
  _Run(messages, lang, stream_cb, complete_cb, error_cb);
}

std::vector<Message> Agent::RunSync(const std::vector<Message> &messages) {
  std::vector<Message> result;
  WFFacilities::WaitGroup wg(1);

  Run(
      messages,
      nullptr, // No streaming
      [&result, &wg](const std::vector<Message> &msgs) {
        result = msgs;
        wg.done();
      },
      [&wg](int, const std::string &) { wg.done(); });

  wg.wait();
  return result;
}

std::vector<Message> Agent::RunSync(const std::string &prompt) {
  Message user_msg(ROLE_USER, prompt);
  return RunSync({user_msg});
}

void Agent::AddTool(ToolBase *tool) {
  if (tool) {
    tools_[tool->name()] = std::unique_ptr<ToolBase>(tool);
  }
}

std::vector<FunctionDef> Agent::getFunctionDefs() const {
  std::vector<FunctionDef> defs;
  for (const auto &pair : tools_) {
    FunctionDef def;
    def.name = pair.second->name();
    def.description = pair.second->description();
    def.parameters = pair.second->parameters();
    defs.push_back(def);
  }
  return defs;
}

void Agent::CallLLM(const std::vector<Message> &messages,
                    const std::vector<FunctionDef> &functions, bool stream,
                    LLMStreamCallback stream_cb,
                    LLMCompleteCallback complete_cb,
                    LLMErrorCallback error_cb) {

  if (!llm_) {
    if (error_cb) {
      error_cb(-1, "LLM not initialized");
    }
    return;
  }

  std::vector<Message> msgs_with_system = PrependSystemMessage(messages);

  if (stream) {
    llm_->ChatStream(msgs_with_system, functions, generate_cfg_, stream_cb,
                     complete_cb, error_cb);
  } else {
    std::vector<Message> result =
        llm_->ChatSync(msgs_with_system, functions, generate_cfg_);
    if (complete_cb) {
      complete_cb(result);
    }
  }
}

ToolResult Agent::CallTool(const std::string &tool_name,
                           const std::string &params) {
  auto it = tools_.find(tool_name);
  if (it == tools_.end()) {
    return ToolResult::error_result("Tool not found: " + tool_name);
  }
  return it->second->Call(params);
}

bool Agent::DetectToolCall(const Message &message, std::string &tool_name,
                           std::string &tool_args) {
  if (message.HasFunctionCall()) {
    tool_name = message.function_call.name;
    tool_args = message.function_call.arguments;
    return true;
  }
  if (message.HasToolCalls() && !message.tool_calls.empty()) {
    tool_name = message.tool_calls[0].function.name;
    tool_args = message.tool_calls[0].function.arguments;
    return true;
  }
  return false;
}

std::string Agent::DetectLanguage(const std::vector<Message> &messages) const {
  for (const auto &msg : messages) {
    for (char c : msg.content) {
      if (static_cast<unsigned char>(c) > 127) {
        return "zh";
      }
    }
  }
  return "en";
}

std::vector<Message>
Agent::PrependSystemMessage(const std::vector<Message> &messages) const {
  if (system_message_.empty()) {
    return messages;
  }

  std::vector<Message> result;
  if (messages.empty() || messages[0].role != ROLE_SYSTEM) {
    result.emplace_back(ROLE_SYSTEM, system_message_);
  } else {
    Message sys_msg = messages[0];
    result.emplace_back(std::move(sys_msg));
  }

  for (size_t i = (messages.empty() || messages[0].role != ROLE_SYSTEM) ? 0 : 1;
       i < messages.size() - 1; ++i) {
    result.emplace_back(messages[i]);
  }
  Message user_msg = messages[messages.size() - 1];
  user_msg.content = "今天是" + getCurrentDate() + "。\n\n" + user_msg.content;
  result.emplace_back(std::move(user_msg));

  return result;
}

// BasicAgent implementation
void BasicAgent::_Run(const std::vector<Message> &messages,
                      const std::string &lang, AgentStreamCallback stream_cb,
                      AgentCompleteCallback complete_cb,
                      AgentErrorCallback error_cb) {

  Message *accumulated = new Message(ROLE_ASSISTANT, "");

  auto llm_stream = [accumulated,
                     stream_cb](const std::vector<Message> &chunk) {
    if (!chunk.empty()) {
      accumulated->content += chunk.back().content;
      accumulated->reasoning_content = chunk.back().reasoning_content;
    }
    if (stream_cb) {
      stream_cb(chunk);
    }
  };

  auto llm_complete = [accumulated,
                       complete_cb](const std::vector<Message> &msgs) {
    if (complete_cb) {
      complete_cb(msgs);
    }
    delete accumulated;
  };

  auto llm_error = [error_cb, accumulated](int code, const std::string &msg) {
    delete accumulated;
    if (error_cb) {
      error_cb(code, msg);
    }
  };

  CallLLM(messages, {}, true, llm_stream, llm_complete, llm_error);
}

Assistant::Assistant(AssistantConfig &&config)
    : Agent(std::move(config)), files_(std::move(config.files)),
      max_llm_calls_per_run_(config.max_llm_calls_per_run) {}

void Assistant::_Run(const std::vector<Message> &messages,
                     const std::string &lang, AgentStreamCallback stream_cb,
                     AgentCompleteCallback complete_cb,
                     AgentErrorCallback error_cb) {

  // Get function definitions for tool calling
  std::vector<FunctionDef> function_defs = getFunctionDefs();

  // Simple implementation: single LLM call, handle tool calls if any
  Message *accumulated = new Message(ROLE_ASSISTANT, "");
  std::vector<Message> *history = new std::vector<Message>(messages);
  int *call_count = new int(0);
  bool *done = new bool(false);

  // Define the recursive LLM call handler
  std::function<void(const std::vector<Message> &)> DoLLMCall =
      [this, &DoLLMCall, history, accumulated, call_count, done, function_defs,
       stream_cb, complete_cb, error_cb](const std::vector<Message> &msgs) {
        if (*done)
          return;
        (*call_count)++;

        if (*call_count > max_llm_calls_per_run_) {
          *done = true;
          if (complete_cb) {
            complete_cb({*accumulated});
          }
          delete accumulated;
          delete history;
          delete call_count;
          delete done;
          return;
        }

        CallLLM(
            msgs, function_defs, true,
            [accumulated, stream_cb](const std::vector<Message> &chunk) {
              if (!chunk.empty()) {
                accumulated->content = chunk.back().content;
                accumulated->reasoning_content = chunk.back().reasoning_content;
              }
              if (stream_cb) {
                stream_cb({*accumulated});
              }
            },
            [this, accumulated, history, call_count, done, function_defs,
             stream_cb, complete_cb, error_cb,
             &DoLLMCall](const std::vector<Message> &response) {
              if (*done)
                return;

              if (!response.empty()) {
                *accumulated = response.back();
                std::string tool_name, tool_args;
                if (DetectToolCall(*accumulated, tool_name, tool_args)) {
                  // Execute tool
                  ToolResult result = CallTool(tool_name, tool_args);

                  // Add assistant message and tool result to history
                  history->push_back(*accumulated);
                  Message tool_msg(ROLE_FUNCTION, result.text);
                  tool_msg.name = tool_name;
                  history->push_back(tool_msg);

                  // Reset accumulated for next LLM call
                  *accumulated = Message(ROLE_ASSISTANT, "");

                  // Recurse
                  DoLLMCall(*history);
                  return;
                }
              }

              *done = true;
              if (complete_cb) {
                complete_cb({*accumulated});
              }
              delete accumulated;
              delete history;
              delete call_count;
              delete done;
            },
            [error_cb, accumulated, history, call_count,
             done](int code, const std::string &msg) {
              *done = true;
              if (error_cb) {
                error_cb(code, msg);
              }
              delete accumulated;
              delete history;
              delete call_count;
              delete done;
            });
      };

  DoLLMCall(messages);
}

NAMESPACE_END
