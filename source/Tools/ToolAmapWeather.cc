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
  return "Get weather information for a Chinese city using Amap (Gaode) API. "
         "Returns current weather conditions and forecast. "
         "Supports city names (e.g., 'Beijing', 'Shanghai', 'Guangzhou') or "
         "city adcode.";
}

Json ToolAmapWeather::parameters() const {
  Json params = Json::Array{};

  Json city_param = Json::Object{};
  city_param["name"] = "city";
  city_param["type"] = "string";
  city_param["description"] = "City name (Chinese or English) or city adcode "
                              "(e.g., 'Beijing', 'Shanghai', '110000')";
  city_param["required"] = true;
  params.push_back(city_param);

  Json extensions_param = Json::Object{};
  extensions_param["name"] = "extensions";
  extensions_param["type"] = "string";
  extensions_param["description"] =
      "Weather data type: 'base' for current weather, 'all' for weather "
      "forecast. Default is 'base'.";
  extensions_param["required"] = false;
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

  std::string normalized_city = NormalizeCity(city);
  std::string url = BuildUrl(normalized_city, extensions);
  FormatInfo("normalized='{}', url='{}'", normalized_city, url);

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

std::string ToolAmapWeather::NormalizeCity(const std::string &city) {
  static std::map<std::string, std::string> city_map = {
      {"Beijing", "北京"},  {"Shanghai", "上海"}, {"Guangzhou", "广州"},
      {"Shenzhen", "深圳"}, {"Hangzhou", "杭州"}, {"Nanjing", "南京"},
      {"Chengdu", "成都"},  {"Wuhan", "武汉"},    {"Xi'an", "西安"},
      {"Xian", "西安"},     {"Tianjin", "天津"},  {"Chongqing", "重庆"},
      {"Harbin", "哈尔滨"}, {"Dalian", "大连"},   {"Qingdao", "青岛"},
      {"Suzhou", "苏州"},   {"Kunming", "昆明"},  {"Fuzhou", "福州"},
      {"Xiamen", "厦门"},   {"Zhengzhou", "郑州"}};

  auto it = city_map.find(city);
  if (it != city_map.end()) {
    return it->second;
  }

  for (char c : city) {
    if (static_cast<unsigned char>(c) > 127) {
      return city;
    }
  }

  return city;
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
  output.push_back("city", api_result.has("city")
                               ? api_result["city"].get<std::string>()
                               : "");

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
