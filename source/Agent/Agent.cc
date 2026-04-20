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
    Json full_def = pair.second->getFunctionDef();
    FunctionDef def;
    def.name = static_cast<std::string>(full_def["name"]);
    if (full_def.has("description")) {
      def.description = static_cast<std::string>(full_def["description"]);
    }
    if (full_def.has("parameters")) {
      def.parameters = full_def["parameters"];
    }
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
  std::vector<Message> result;
  std::string date_prefix = "今天是" + getCurrentDate() + "。\n\n";

  // 1. Handle system message
  bool has_system = false;
  if (!messages.empty() && messages[0].role == ROLE_SYSTEM) {
    has_system = true;
    Message sys_msg = messages[0];
    if (!system_message_.empty() && sys_msg.content.find(system_message_) == std::string::npos) {
        sys_msg.content = system_message_ + "\n" + sys_msg.content;
    }
    // Add date to system message or first user message? 
    // Let's keep system message clean and add to first user message.
    result.push_back(std::move(sys_msg));
  } else if (!system_message_.empty()) {
    result.push_back(Message(ROLE_SYSTEM, system_message_));
  }

  // 2. Add remaining messages and inject date into the FIRST user message
  bool date_injected = false;
  size_t start_idx = has_system ? 1 : 0;
  
  for (size_t i = start_idx; i < messages.size(); ++i) {
    Message msg = messages[i];
    if (!date_injected && msg.role == ROLE_USER) {
      if (msg.content.find(date_prefix) == std::string::npos) {
        msg.content = date_prefix + msg.content;
      }
      date_injected = true;
    }
    result.push_back(std::move(msg));
  }

  // If no user message found and date not injected, maybe append to system message if it exists
  if (!date_injected && !result.empty() && result[0].role == ROLE_SYSTEM) {
      result[0].content = date_prefix + result[0].content;
  }

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

  struct RunContext {
    std::vector<Message> history;
    Message accumulated;
    int call_count = 0;
    bool done = false;
    std::vector<FunctionDef> function_defs;
    AgentStreamCallback stream_cb;
    AgentCompleteCallback complete_cb;
    AgentErrorCallback error_cb;
    std::function<void(const std::vector<Message> &)> do_llm_call;

    RunContext(const std::vector<Message> &msgs,
               std::vector<FunctionDef> &&defs, AgentStreamCallback s,
               AgentCompleteCallback c, AgentErrorCallback e)
        : history(msgs), accumulated(ROLE_ASSISTANT, ""),
          function_defs(std::move(defs)), stream_cb(s), complete_cb(c),
          error_cb(e) {}
  };

  auto ctx = std::make_shared<RunContext>(messages, getFunctionDefs(),
                                         stream_cb, complete_cb, error_cb);

  ctx->do_llm_call = [this, ctx](const std::vector<Message> &msgs) {
    if (ctx->done)
      return;
    ctx->call_count++;

    if (ctx->call_count > max_llm_calls_per_run_) {
      ctx->done = true;
      if (ctx->complete_cb) {
        ctx->complete_cb({ctx->accumulated});
      }
      ctx->do_llm_call = nullptr; // Break cycle
      return;
    }

    CallLLM(
        msgs, ctx->function_defs, true,
        [ctx](const std::vector<Message> &chunk) {
          if (!chunk.empty()) {
            ctx->accumulated.content += chunk.back().content;
            ctx->accumulated.reasoning_content += chunk.back().reasoning_content;
          }
          if (ctx->stream_cb) {
            ctx->stream_cb({ctx->accumulated});
          }
        },
        [this, ctx](const std::vector<Message> &response) {
          if (ctx->done)
            return;

          if (!response.empty()) {
            const Message &resp_info = response.back();
            bool tool_triggered = false;

            if (resp_info.HasToolCalls()) {
              ctx->history.push_back(resp_info);
              for (const auto &tc : resp_info.tool_calls) {
                ToolResult result = CallTool(tc.function.name, tc.function.arguments);
                std::string result_text = result.success ? result.text : result.error;
                Message tool_msg(ROLE_TOOL, result_text);
                tool_msg.tool_call_id = tc.id;
                ctx->history.push_back(tool_msg);
                tool_triggered = true;
              }
            } else if (resp_info.HasFunctionCall()) {
              ctx->history.push_back(resp_info);
              ToolResult result = CallTool(resp_info.function_call.name, resp_info.function_call.arguments);
              std::string result_text = result.success ? result.text : result.error;
              Message tool_msg(ROLE_TOOL, result_text);
              tool_msg.tool_call_id = "fc_" + resp_info.function_call.name;
              ctx->history.push_back(tool_msg);
              tool_triggered = true;
            }

            if (tool_triggered) {
              ctx->accumulated = Message(ROLE_ASSISTANT, "");
              ctx->do_llm_call(ctx->history);
              return;
            }
          }

          ctx->done = true;
          if (ctx->complete_cb) {
            ctx->complete_cb({ctx->accumulated});
          }
          ctx->do_llm_call = nullptr; // Break cycle
        },
        [ctx](int code, const std::string &msg) {
          ctx->done = true;
          if (ctx->error_cb) {
            ctx->error_cb(code, msg);
          }
          ctx->do_llm_call = nullptr; // Break cycle
        });
  };

  ctx->do_llm_call(ctx->history);
}

NAMESPACE_END
