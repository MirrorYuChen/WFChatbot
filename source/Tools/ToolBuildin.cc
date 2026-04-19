/*
 * @Author: chenjingyu
 * @Date: 2026-04-18 21:39:13
 * @Contact: 2458006466@qq.com
 * @Description: ToolBuildin
 */
#include "Tools/ToolBuildin.h"

NAMESPACE_BEGIN
std::string ToolWebSearch::name() const { return "web_search"; }
std::string ToolWebSearch::description() const {
  return "Search the web for current information. Returns relevant search "
         "results.";
}

Json ToolWebSearch::parameters() const {
  Json params = Json::Array{};
  Json query_param = Json::Object{};
  query_param["name"] = "query";
  query_param["type"] = "string";
  query_param["description"] = "The search query";
  query_param["required"] = true;
  params.push_back(query_param);
  return params;
}

ToolResult ToolWebSearch::Call(const std::string &params,
                               const Json &kwargs) {
  Json p = ParseParams(params);
  if (!p.has("query")) {
    return ToolResult::error_result("Missing 'query' parameter");
  }
  std::string query = static_cast<std::string>(p["query"]);
  // Placeholder - in production would call actual search API
  Json result = Json::Object{};
  result["query"] = query;
  result["results"] = Json::Array{};
  result["message"] = "Web search not yet implemented. Return query for now.";
  return ToolResult(result.dump());
}

std::string ToolCurrentTime::name() const { return "current_time"; }
std::string ToolCurrentTime::description() const {
  return "Get the current date and time.";
}

Json ToolCurrentTime::parameters() const { return Json::Array{}; }

ToolResult ToolCurrentTime::Call(const std::string &params,
                                 const Json &kwargs) {
  time_t now = time(nullptr);
  struct tm *tm_info = localtime(&now);
  char buffer[128];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S %Z", tm_info);
  Json result = Json::Object{};
  result["datetime"] = std::string(buffer);
  return ToolResult(result.dump());
}

std::string ToolCalculator::name() const { return "calculator"; }
std::string ToolCalculator::description() const {
  return "Evaluate a mathematical expression.";
}

Json ToolCalculator::parameters() const {
  Json params = Json::Array{};
  Json expr_param = Json::Object{};
  expr_param["name"] = "expression";
  expr_param["type"] = "string";
  expr_param["description"] = "The mathematical expression to evaluate";
  expr_param["required"] = true;
  params.push_back(expr_param);
  return params;
}

ToolResult ToolCalculator::Call(const std::string &params,
                                const Json &kwargs) {
  Json p = ParseParams(params);
  if (!p.has("expression")) {
    return ToolResult::error_result("Missing 'expression' parameter");
  }
  std::string expr = static_cast<std::string>(p["expression"]);
  // Placeholder - in production would use a safe math evaluator
  Json result = Json::Object{};
  result["expression"] = expr;
  result["result"] =
      "Calculator not yet implemented. Return expression for now.";
  return ToolResult(result.dump());
}

NAMESPACE_END
