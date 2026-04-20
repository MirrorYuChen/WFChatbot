/*
 * @Author: chenjingyu
 * @Date: 2026-04-18 21:47:22
 * @Contact: 2458006466@qq.com
 * @Description: ToolAmapWeather
 */
#include "Tools/ToolAmapWeather.h"
#include "Base/Logger.h"

NAMESPACE_BEGIN
ToolAmapWeather::ToolAmapWeather(const Json &cfg) : cfg_(cfg) {
  if (cfg.has("api_key")) {
    api_key_ = static_cast<std::string>(cfg["api_key"]);
  } else {
    const char *env_key = std::getenv("AMAP_API_KEY");
    if (env_key) {
      api_key_ = env_key;
    }
  }
}

std::string ToolAmapWeather::name() const { return "amap_weather"; }

std::string ToolAmapWeather::description() const {
  return "获取对应城市的天气数据";
}

Json ToolAmapWeather::parameters() const {
  Json params = Json::Array{};

  Json city_param = Json::Object{};
  city_param.push_back("name", "city");
  city_param.push_back("type", "string");
  city_param.push_back("description", "城市/区具体名称，如`北京市海淀区`请描述为`海淀区`，务必以市或区结尾");
  city_param.push_back("required", true);
  params.push_back(city_param);

  Json extensions_param = Json::Object{};
  extensions_param.push_back("name", "extensions");
  extensions_param.push_back("type", "string");
  extensions_param.push_back("description",
                             "Weather data type: 'base' for current weather, "
                             "'all' for weather forecast. Default is 'base'.");
  extensions_param.push_back("required", false);
  params.push_back(extensions_param);

  return params;
}

ToolResult ToolAmapWeather::Call(const std::string &params,
                                 const Json &kwargs) {
  FormatInfo("[CALL] ToolAmapWeather::call ENTERED");

  Json p = ParseParams(params);

  if (!p.has("city")) {
    FormatError("Missing city parameter");
    return ToolResult::error_result(
        "Missing 'city' parameter. Please provide a city name or adcode.");
  }

  if (api_key_.empty()) {
    FormatError("API key empty");
    return ToolResult::error_result(
        "Amap API key not configured. Please set AMAP_API_KEY environment "
        "variable or provide api_key in tool configuration.");
  }

  std::string city = static_cast<std::string>(p["city"]);
  std::string extensions = "base";
  if (p.has("extensions")) {
    extensions = static_cast<std::string>(p["extensions"]);
  }
  FormatInfo("Tool call] city='{}', extensions='{}', api_key='{}'", city,
             extensions, api_key_);
  std::string url = BuildUrl(city, extensions);
  FormatInfo("normalized='{}', url='{}'", city, url);

  std::string response;

  WFFacilities::WaitGroup wg(1);

  WFHttpTask *http_task = WFTaskFactory::create_http_task(
      url, 0, 0, [&response, &wg](WFHttpTask *task) {
        int state = task->get_state();
        if (state == WFT_STATE_SUCCESS) {
          protocol::HttpResponse *resp = task->get_resp();
          const void *body;
          size_t body_len;
          resp->get_parsed_body(&body, &body_len);
          response.assign(static_cast<const char *>(body), body_len);
        }
        wg.done();
      });

  http_task->start();
  wg.wait();
  FormatInfo("HTTP response: {}", response);

  if (response.empty()) {
    return ToolResult::error_result(
        "Failed to fetch weather data from Amap API");
  }

  Json api_result = Json::parse(response);
  if (!api_result.is_valid()) {
    return ToolResult::error_result("Failed to parse weather data: " +
                                    response);
  }

  FormatInfo("Parsed JSON: status='{}', lives_count='{}",
             api_result.has("status")
                 ? api_result["status"].get<std::string>().c_str()
                 : "missing",
             api_result.has("lives") && api_result["lives"].is_array()
                 ? api_result["lives"].size()
                 : 0);

  if (api_result.has("status") &&
      api_result["status"].get<std::string>() != "1") {
    std::string info = api_result.has("info")
                           ? api_result["info"].get<std::string>()
                           : "Unknown error";
    return ToolResult::error_result("Amap API error: " + info);
  }

  if (extensions == "base") {
    if (!api_result.has("lives") || !api_result["lives"].is_array() ||
        api_result["lives"].size() == 0) {
      return ToolResult::error_result(
          "No weather data found for city '" + city +
          "'. Please try using Chinese city name (e.g., '北京') or city "
          "adcode (e.g., '110000'). Supported English names: Beijing, "
          "Shanghai, Guangzhou, etc.");
    }
  } else {
    if (!api_result.has("forecasts") || !api_result["forecasts"].is_array() ||
        api_result["forecasts"].size() == 0) {
      return ToolResult::error_result("No forecast data found for city '" +
                                      city +
                                      "'. Please try using Chinese city name "
                                      "(e.g., '北京') or city adcode.");
    }
  }

  Json output = FormatWeatherResult(api_result, extensions);
  return ToolResult(output.dump());
}

std::string ToolAmapWeather::BuildUrl(const std::string &city,
                                      const std::string &extensions) {
  std::string url = "https://restapi.amap.com/v3/weather/weatherInfo?";
  url += "key=" + api_key_;
  url += "&city=" + city;
  url += "&extensions=" + extensions;
  url += "&output=JSON";
  return url;
}

Json ToolAmapWeather::FormatWeatherResult(const Json &api_result,
                                          const std::string &extensions) {
  Json output = Json::Object{};
  output.push_back("status", "success");
  std::string city_name = "";
  if (extensions == "base") {
    if (api_result.has("lives") && api_result["lives"].size() > 0) {
      city_name = api_result["lives"][0].has("city") ? 
                  static_cast<std::string>(api_result["lives"][0]["city"]) : "";
    }
  } else {
    if (api_result.has("forecasts") && api_result["forecasts"].size() > 0) {
      city_name = api_result["forecasts"][0].has("city") ? 
                  static_cast<std::string>(api_result["forecasts"][0]["city"]) : "";
    }
  }
  
  output.push_back("city", city_name);

  if (extensions == "base") {
    if (api_result.has("lives") && api_result["lives"].is_array() &&
        api_result["lives"].size() > 0) {
      const Json &live = api_result["lives"][0];
      Json current = Json::Object{};
      current.push_back("weather", live.has("weather")
                                       ? live["weather"].get<std::string>()
                                       : "");
      current.push_back("temperature",
                        live.has("temperature")
                            ? live["temperature"].get<std::string>()
                            : "");
      current.push_back("wind_direction",
                        live.has("winddirection")
                            ? live["winddirection"].get<std::string>()
                            : "");
      current.push_back("wind_power", live.has("windpower")
                                          ? live["windpower"].get<std::string>()
                                          : "");
      current.push_back("humidity", live.has("humidity")
                                        ? live["humidity"].get<std::string>()
                                        : "");
      current.push_back(
          "report_time",
          live.has("reporttime") ? live["reporttime"].get<std::string>() : "");
      output.push_back("current", current);
    }
  } else {
    if (api_result.has("forecasts") && api_result["forecasts"].is_array() &&
        api_result["forecasts"].size() > 0) {
      const Json &forecast_data = api_result["forecasts"][0];
      if (forecast_data.has("casts") && forecast_data["casts"].is_array()) {
        Json forecasts = Json::Array{};
        for (size_t i = 0; i < forecast_data["casts"].size(); ++i) {
          const Json &cast = forecast_data["casts"][i];
          Json day = Json::Object{};
          day.push_back(
              "date", cast.has("date") ? cast["date"].get<std::string>() : "");
          day.push_back(
              "week", cast.has("week") ? cast["week"].get<std::string>() : "");
          day.push_back("day_weather",
                        cast.has("dayweather")
                            ? cast["dayweather"].get<std::string>()
                            : "");
          day.push_back("night_weather",
                        cast.has("nightweather")
                            ? cast["nightweather"].get<std::string>()
                            : "");
          day.push_back("day_temp", cast.has("daytemp")
                                        ? cast["daytemp"].get<std::string>()
                                        : "");
          day.push_back("night_temp", cast.has("nighttemp")
                                          ? cast["nighttemp"].get<std::string>()
                                          : "");
          day.push_back("day_wind_direction",
                        cast.has("daywind") ? cast["daywind"].get<std::string>()
                                            : "");
          day.push_back("night_wind_direction",
                        cast.has("nightwind")
                            ? cast["nightwind"].get<std::string>()
                            : "");
          day.push_back(
              "day_wind_power",
              cast.has("daypower") ? cast["daypower"].get<std::string>() : "");
          day.push_back("night_wind_power",
                        cast.has("nightpower")
                            ? cast["nightpower"].get<std::string>()
                            : "");
          forecasts.push_back(day);
        }
        output.push_back("forecasts", forecasts);
      }
    }
  }

  return output;
}

REGISTERTOOL("amap_weather", ToolAmapWeather)

NAMESPACE_END
