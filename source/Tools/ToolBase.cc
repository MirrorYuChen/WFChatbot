/*
 * @Author: chenjingyu
 * @Date: 2026-04-18 21:32:40
 * @Contact: 2458006466@qq.com
 * @Description: ToolBase
 */
#include "Tools/ToolBase.h"
#include <iostream>
#include <sstream>
#include <typeinfo>

NAMESPACE_BEGIN
ToolBase::ToolBase() {}

Json ToolBase::getFunctionDef() const {
  Json def = Json::Object{};
  def["name"] = name();
  def["description"] = description();

  Json params = parameters();
  if (params.is_null() || params.size() == 0) {
    // Default empty parameters
    def["parameters"] = Json::Object{{"type", "object"},
                                     {"properties", Json::Object{}},
                                     {"required", Json::Array{}}};
  } else if (params.is_array()) {
    // Convert array format to JSON schema
    Json properties = Json::Object{};
    Json required = Json::Array{};
    for (size_t i = 0; i < params.size(); ++i) {
      const Json &param = params[i];
      Json prop = Json::Object{};
      if (param.has("type")) {
        prop["type"] = param["type"];
      } else {
        prop["type"] = "string";
      }
      if (param.has("description")) {
        prop["description"] = param["description"];
      }
      properties[param["name"].get<std::string>()] = prop;

      if (param.has("required") && static_cast<bool>(param["required"])) {
        required.push_back(param["name"]);
      }
    }
    def["parameters"] = Json::Object{
        {"type", "object"}, {"properties", properties}, {"required", required}};
  } else {
    def["parameters"] = params;
  }

  return def;
}

bool ToolBase::ValidateParams(const std::string &params,
                               std::string &error_msg) const {
  Json json_params = Json::parse(params);
  if (!json_params.is_valid()) {
    error_msg = "Invalid JSON parameters";
    return false;
  }

  // Basic validation - check required fields
  Json params_schema = parameters();
  if (params_schema.is_array()) {
    for (size_t i = 0; i < params_schema.size(); ++i) {
      if (params_schema[i].has("required") &&
          static_cast<bool>(params_schema[i]["required"])) {
        std::string param_name =
            static_cast<std::string>(params_schema[i]["name"]);
        if (!json_params.has(param_name)) {
          error_msg = "Missing required parameter: " + param_name;
          return false;
        }
      }
    }
  }

  return true;
}

Json ToolBase::ParseParams(const std::string &params) const {
  Json json = Json::parse(params);
  if (!json.is_valid()) {
    return Json::Object{};
  }
  return json;
}

bool ToolBase::CheckRequired(const Json &params,
                              const std::vector<std::string> &required) const {
  for (const auto &req : required) {
    if (!params.has(req)) {
      return false;
    }
  }
  return true;
}

// Tool Registry implementation
ToolRegistry &ToolRegistry::instance() {
  static ToolRegistry registry;
  return registry;
}

void ToolRegistry::RegisterTool(const std::string &name, ToolCreator creator) {
  creators_[name] = creator;
}

ToolBase *ToolRegistry::CreateTool(const std::string &name, const Json &cfg) {
  auto it = creators_.find(name);
  if (it != creators_.end()) {
    ToolBase *tool = it->second(cfg);
    std::cout << "[Registry] Created tool '" << name << "' ptr=" << tool
              << " type-info=" << typeid(*tool).name() << std::endl
              << std::flush;
    if (tool) {
      std::cout << "[Registry] Tool name()='" << tool->name() << "'"
                << std::endl
                << std::flush;
    }
    return tool;
  }
  std::cout << "[Registry] Tool '" << name << "' NOT FOUND" << std::endl
            << std::flush;
  return nullptr;
}

bool ToolRegistry::HasTool(const std::string &name) const {
  return creators_.find(name) != creators_.end();
}

std::vector<std::string> ToolRegistry::ListTools() const {
  std::vector<std::string> names;
  for (const auto &pair : creators_) {
    names.push_back(pair.first);
  }
  return names;
}

NAMESPACE_END
