/*
 * @Author: chenjingyu
 * @Date: 2026-04-18 23:21:14
 * @Contact: 2458006466@qq.com
 * @Description: Agent
 */
#pragma once

#include "Api.h"

#include "Base/Json.h"
#include "LLM/LLMBase.h"
#include "LLM/LLMSchema.h"
#include "Tools/ToolBase.h"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

NAMESPACE_BEGIN
// Agent response callback types
using AgentStreamCallback = std::function<void(const std::vector<Message> &)>;
using AgentCompleteCallback = std::function<void(const std::vector<Message> &)>;
using AgentErrorCallback = std::function<void(int, const std::string &)>;

// Base Agent class - inspired by Qwen-Agent's Agent design
class API Agent {
public:
  struct Config {
    Json llm_cfg;                   // LLM configuration (used with registry)
    std::unique_ptr<LLMBase> llm;   // Direct LLM instance (bypasses registry)
    std::vector<std::string> tools; // Tool names
    std::vector<Json> tool_configs; // Tool configurations
    std::vector<ToolBase *> custom_tools; // Custom tool instances
    std::string system_message = DEFAULT_SYSTEM_MESSAGE;
    std::string name;
    std::string description;
    GenerateConfig generate_cfg;
  };

  Agent(Config &&config);
  virtual ~Agent() = default;

  // Run agent with messages (async, streaming)
  void Run(const std::vector<Message> &messages, AgentStreamCallback stream_cb,
           AgentCompleteCallback complete_cb, AgentErrorCallback error_cb);

  // Run agent with messages (sync, blocking)
  std::vector<Message> RunSync(const std::vector<Message> &messages);

  // Run agent with simple prompt
  std::vector<Message> RunSync(const std::string &prompt);

  // Add a tool dynamically
  void AddTool(ToolBase *tool);

  // Get available tools as function definitions
  std::vector<FunctionDef> getFunctionDefs() const;

  // Access LLM
  LLMBase *llm() const { return llm_.get(); }

  // Get agent name and description
  const std::string &name() const { return name_; }
  const std::string &description() const { return description_; }

protected:
  // Pure virtual: agent's main workflow
  virtual void _Run(const std::vector<Message> &messages,
                    const std::string &lang, AgentStreamCallback stream_cb,
                    AgentCompleteCallback complete_cb,
                    AgentErrorCallback error_cb) = 0;

  // Call LLM (helper for subclasses)
  void CallLLM(const std::vector<Message> &messages,
                const std::vector<FunctionDef> &functions, bool stream,
                LLMStreamCallback stream_cb, LLMCompleteCallback complete_cb,
                LLMErrorCallback error_cb);

  // Call a tool by name
  ToolResult CallTool(const std::string &tool_name, const std::string &params);

  // Detect if message requires tool call
  bool DetectToolCall(const Message &message, std::string &tool_name,
                        std::string &tool_args);

  // Detect language of messages
  std::string DetectLanguage(const std::vector<Message> &messages) const;

  // Prepend system message if needed
  std::vector<Message>
  PrependSystemMessage(const std::vector<Message> &messages) const;

protected:
  std::unique_ptr<LLMBase> llm_;
  std::map<std::string, std::unique_ptr<ToolBase>> tools_;
  std::string system_message_;
  std::string name_;
  std::string description_;
  GenerateConfig generate_cfg_;
};

// BasicAgent - simplest agent that just uses LLM without tools
class API BasicAgent : public Agent {
public:
  BasicAgent(Config &&config) : Agent(std::move(config)) {}

protected:
  void _Run(const std::vector<Message> &messages, const std::string &lang,
            AgentStreamCallback stream_cb, AgentCompleteCallback complete_cb,
            AgentErrorCallback error_cb) override;
};

// Assistant agent - supports tool use and file reading (like Qwen-Agent's
// Assistant)
class API Assistant : public Agent {
public:
  struct AssistantConfig : public Agent::Config {
    std::vector<std::string> files; // Files for the agent to read
    int max_llm_calls_per_run = 10; // Maximum LLM calls per Run
  };

  Assistant(AssistantConfig &&config);

protected:
  void _Run(const std::vector<Message> &messages, const std::string &lang,
            AgentStreamCallback stream_cb, AgentCompleteCallback complete_cb,
            AgentErrorCallback error_cb) override;

private:
  std::vector<std::string> files_;
  int max_llm_calls_per_run_;
};

NAMESPACE_END
