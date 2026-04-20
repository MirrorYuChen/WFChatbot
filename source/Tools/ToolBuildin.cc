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
  query_param.push_back("name", "query");
  query_param.push_back("type", "string");
  query_param.push_back("description", "The search query");
  query_param.push_back("required", true);
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
  result.push_back("query", query);
  result.push_back("results", Json::Array{});
  result.push_back("message", "Web search not yet implemented. Return query for now.");
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
  result.push_back("datetime", std::string(buffer));
  return ToolResult(result.dump());
}

std::string ToolCalculator::name() const { return "calculator"; }
std::string ToolCalculator::description() const {
  return "Evaluate a mathematical expression.";
}

Json ToolCalculator::parameters() const {
  Json params = Json::Array{};
  Json expr_param = Json::Object{};
  expr_param.push_back("name", "expression");
  expr_param.push_back("type", "string");
  expr_param.push_back("description", "The mathematical expression to evaluate");
  expr_param.push_back("required", true);
  params.push_back(expr_param);
  return params;
}

namespace {
// Simple math evaluator for basic arithmetic
class MathEvaluator {
public:
  double eval(const std::string &expr) {
    it_ = expr.begin();
    end_ = expr.end();
    return parseExpression();
  }

private:
  double parseExpression() {
    double res = parseTerm();
    while (it_ != end_) {
      skipSpaces();
      if (it_ == end_) break;
      if (*it_ == '+') { ++it_; res += parseTerm(); }
      else if (*it_ == '-') { ++it_; res -= parseTerm(); }
      else break;
    }
    return res;
  }

  double parseTerm() {
    double res = parseFactor();
    while (it_ != end_) {
      skipSpaces();
      if (it_ == end_) break;
      if (*it_ == '*') { ++it_; res *= parseFactor(); }
      else if (*it_ == '/') {
        ++it_;
        double den = parseFactor();
        if (den == 0) throw std::runtime_error("Division by zero");
        res /= den;
      }
      else break;
    }
    return res;
  }

  double parseFactor() {
    skipSpaces();
    if (it_ != end_ && *it_ == '(') {
      ++it_;
      double res = parseExpression();
      skipSpaces();
      if (it_ != end_ && *it_ == ')') ++it_;
      return res;
    }
    if (it_ != end_ && *it_ == '-') {
      ++it_;
      return -parseFactor();
    }
    return parseNumber();
  }

  double parseNumber() {
    skipSpaces();
    std::string s;
    while (it_ != end_ && (isdigit(*it_) || *it_ == '.')) {
      s += *it_;
      ++it_;
    }
    if (s.empty()) return 0;
    return std::stod(s);
  }

  void skipSpaces() {
    while (it_ != end_ && isspace(*it_)) ++it_;
  }

  std::string::const_iterator it_;
  std::string::const_iterator end_;
};
} // namespace

ToolResult ToolCalculator::Call(const std::string &params,
                                const Json &kwargs) {
  Json p = ParseParams(params);
  if (!p.has("expression")) {
    return ToolResult::error_result("Missing 'expression' parameter");
  }
  std::string expr = static_cast<std::string>(p["expression"]);
  
  try {
    MathEvaluator eval;
    double res = eval.eval(expr);
    
    // Check if it's an integer
    Json result = Json::Object{};
    result.push_back("expression", expr);
    if (res == (long long)res) {
      result.push_back("result", (long long)res);
    } else {
      result.push_back("result", res);
    }
    result.push_back("status", "success");
    return ToolResult(result.dump());
  } catch (const std::exception &e) {
    return ToolResult::error_result(std::string("Math evaluation error: ") + e.what());
  }
}

REGISTERTOOL("web_search", ToolWebSearch)
REGISTERTOOL("current_time", ToolCurrentTime)
REGISTERTOOL("calculator", ToolCalculator)

NAMESPACE_END
