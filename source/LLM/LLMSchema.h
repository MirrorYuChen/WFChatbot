/*
 * @Author: chenjingyu
 * @Date: 2026-04-18 19:54:54
 * @Contact: 2458006466@qq.com
 * @Description: LLMSchema
 */
#pragma once

#include "Api.h"
#include "Base/Json.h"
#include "Base/Logger.h"

#include <ctime>
#include <optional>
#include <string>
#include <variant>
#include <vector>

NAMESPACE_BEGIN
// Role constants
inline const std::string ROLE_SYSTEM = "system";
inline const std::string ROLE_USER = "user";
inline const std::string ROLE_ASSISTANT = "assistant";
inline const std::string ROLE_FUNCTION = "function";
inline const std::string ROLE_TOOL = "tool";

// Get current date string in "YYYY年MM月DD日" format
inline API std::string getCurrentDate() {
  time_t now = time(nullptr);
  struct tm *tm_info = localtime(&now);
  char buffer[64];
  strftime(buffer, sizeof(buffer), "%Y年%m月%d日", tm_info);
  FormatInfo("Current date: {}", buffer);
  return std::string(buffer);
}

// Default system message
inline API const std::string DEFAULT_SYSTEM_MESSAGE =
    "You are a helpful assistant. "
    "When responding, use the following guidelines:\n"
    "- Be accurate and helpful.\n"
    "- Provide clear and concise responses.\n"
    "- If you don't know something, admit it.\n"
    "- Be friendly and professional.\n";

// Content item for multimodal support
struct API ContentItem {
  std::string type; // "text", "image", "audio", etc.
  std::string text;
  std::string image_url;
  std::string audio_url;

  ContentItem() = default;
  explicit ContentItem(const std::string &t) : type("text"), text(t) {}

  bool isText() const { return type == "text"; }
  bool isImage() const { return type == "image"; }
  bool isAudio() const { return type == "audio"; }

  std::string getText() const {
    if (type == "text")
      return text;
    return "";
  }

  Json ToJson() const {
    Json j = Json::Object{};
    j["type"] = type;
    if (type == "text") {
      j["text"] = text;
    } else if (type == "image") {
      j["image_url"] = image_url;
    } else if (type == "audio") {
      j["audio_url"] = audio_url;
    }
    return j;
  }

  static ContentItem FromJson(const Json &j) {
    ContentItem item;
    if (j.has("type")) {
      item.type = static_cast<std::string>(j["type"]);
    }
    if (j.has("text")) {
      item.text = static_cast<std::string>(j["text"]);
    }
    if (j.has("image_url")) {
      item.image_url = static_cast<std::string>(j["image_url"]);
    }
    if (j.has("audio_url")) {
      item.audio_url = static_cast<std::string>(j["audio_url"]);
    }
    return item;
  }
};

// Function call structure
struct API FunctionCall {
  std::string name;
  std::string arguments;

  Json ToJson() const {
    Json j = Json::Object{};
    j["name"] = name;
    j["arguments"] = arguments;
    return j;
  }

  static FunctionCall FromJson(const Json &j) {
    FunctionCall fc;
    if (j.has("name")) {
      fc.name = static_cast<std::string>(j["name"]);
    }
    if (j.has("arguments")) {
      fc.arguments = static_cast<std::string>(j["arguments"]);
    }
    return fc;
  }
};

// Tool call structure
struct API ToolCall {
  std::string id;
  std::string type = "function";
  FunctionCall function;

  Json ToJson() const {
    Json j = Json::Object{};
    j["id"] = id;
    j["type"] = type;
    j["function"] = function.ToJson();
    return j;
  }

  static ToolCall FromJson(const Json &j) {
    ToolCall tc;
    if (j.has("id")) {
      tc.id = static_cast<std::string>(j["id"]);
    }
    if (j.has("type")) {
      tc.type = static_cast<std::string>(j["type"]);
    }
    if (j.has("function")) {
      tc.function = FunctionCall::FromJson(j["function"]);
    }
    return tc;
  }
};

// Message structure mirroring Qwen-Agent's Message schema
struct API Message {
  std::string role;
  std::string content;
  std::string reasoning_content;
  std::string name;
  FunctionCall function_call;
  std::vector<ToolCall> tool_calls;
  Json extra; // Extra metadata

  Message() = default;
  Message(const std::string &r, const std::string &c) : role(r), content(c) {}

  bool HasContent() const { return !content.empty(); }
  bool HasFunctionCall() const { return !function_call.name.empty(); }
  bool HasToolCalls() const { return !tool_calls.empty(); }
  bool HasReasoning() const { return !reasoning_content.empty(); }

  // Convert to JSON
  Json ToJson() const {
    Json j = Json::Object{};
    j["role"] = role;

    if (!content.empty()) {
      // Try to parse as JSON array for multimodal content
      Json content_json = Json::parse(content);
      if (content_json.is_array()) {
        j["content"] = content_json;
      } else {
        j["content"] = content;
      }
    }

    if (!reasoning_content.empty()) {
      j["reasoning_content"] = reasoning_content;
    }
    if (!name.empty()) {
      j["name"] = name;
    }
    if (HasFunctionCall()) {
      j["function_call"] = function_call.ToJson();
    }
    if (HasToolCalls()) {
      Json tc_array = Json::Array{};
      for (const auto &tc : tool_calls) {
        tc_array.push_back(tc.ToJson());
      }
      j["tool_calls"] = tc_array;
    }
    if (!extra.is_null()) {
      j["extra"] = extra;
    }
    return j;
  }

  // Create from JSON
  static Message FromJson(const Json &j) {
    Message msg;
    if (j.has("role")) {
      msg.role = static_cast<std::string>(j["role"]);
    }
    if (j.has("content")) {
      if (j["content"].is_string()) {
        msg.content = static_cast<std::string>(j["content"]);
      } else {
        msg.content = j["content"].dump();
      }
    }
    if (j.has("reasoning_content")) {
      msg.reasoning_content = static_cast<std::string>(j["reasoning_content"]);
    }
    if (j.has("name")) {
      msg.name = static_cast<std::string>(j["name"]);
    }
    if (j.has("function_call")) {
      msg.function_call = FunctionCall::FromJson(j["function_call"]);
    }
    if (j.has("tool_calls") && j["tool_calls"].is_array()) {
      for (size_t i = 0; i < j["tool_calls"].size(); ++i) {
        msg.tool_calls.push_back(ToolCall::FromJson(j["tool_calls"][i]));
      }
    }
    if (j.has("extra")) {
      msg.extra = j["extra"].copy();
    }
    return msg;
  }
};

// Function definition for tool calling
struct API FunctionDef {
  std::string name;
  std::string description;
  Json parameters; // JSON Schema

  Json ToJson() const {
    Json j = Json::Object{};
    j["name"] = name;
    j["description"] = description;
    j["parameters"] = parameters;
    return j;
  }

  static FunctionDef FromJson(const Json &j) {
    FunctionDef fd;
    if (j.has("name")) {
      fd.name = static_cast<std::string>(j["name"]);
    }
    if (j.has("description")) {
      fd.description = static_cast<std::string>(j["description"]);
    }
    if (j.has("parameters")) {
      fd.parameters = j["parameters"].copy();
    }
    return fd;
  }
};

NAMESPACE_END
