/*
 * @Author: chenjingyu
 * @Date: 2026-04-18 21:47:16
 * @Contact: 2458006466@qq.com
 * @Description: ToolAmapWeather
 */
#pragma once

#include "Api.h"

#include "Base/Json.h"
#include "Tools/ToolBase.h"

#include <workflow/HttpMessage.h>
#include <workflow/WFFacilities.h>
#include <workflow/WFTaskFactory.h>

#include <cstdlib>
#include <map>
#include <string>
#include <unistd.h>

NAMESPACE_BEGIN
class ToolAmapWeather : public ToolBase {
public:
  explicit ToolAmapWeather(const Json &cfg = Json());

  std::string name() const override;

  std::string description() const override;

  Json parameters() const override;

  ToolResult Call(const std::string &params,
                  const Json &kwargs = Json()) override;

private:
  Json cfg_;
  std::string api_key_;

  std::string NormalizeCity(const std::string &city);

  std::string BuildUrl(const std::string &city, const std::string &extensions);

  Json FormatWeatherResult(const Json &api_result,
                           const std::string &extensions);
};

NAMESPACE_END
