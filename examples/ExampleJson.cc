/*
 * @Author: chenjingyu
 * @Date: 2026-02-27 17:21:45
 * @Contact: 2458006466@qq.com
 * @Description: Json
 */
#include "Base/Json.h"
#include "Base/Logger.h"

using namespace NAMESPACE;

int main(int argc, char *argv[]) {
  {
    Json data = Json::Array{1, true, "string", nullptr, "123"};
    FormatInfo(data.dump());
  }

  {
    Json data;
    data.push_back(1);
    data.push_back(nullptr);
    data.push_back("string");
    data.push_back(true);
    data.push_back(false);
    FormatInfo(data.dump());
  }

  {
    Json data;
    data.push_back(1);
    data.push_back(nullptr);
    data.push_back("string");
    data.push_back(true);
    data.push_back(false);

    FormatInfo(data.dump());
    data[2] = 1;
    StreamInfo << data[2].get<int>();
    data[2] = 2.0;
    StreamInfo << data[2].get<int>();
    data[2] = "123";
    StreamInfo << data[2].get<std::string>();
    data[2] = nullptr;
    StreamInfo << data[2].get<std::nullptr_t>();
  }

  {
    Json data;
    data.push_back(1);        // 0
    data.push_back(2.1);      // 1
    data.push_back(nullptr);  // 2
    data.push_back("string"); // 3
    data.push_back(true);     // 4
    data.push_back(false);    // 5

    Json::Object obj;
    obj["123"] = 12;
    obj["123"]["1"] = "test";
    data.push_back(obj);

    Json::Array arr;
    arr.push_back(1);
    arr.push_back(nullptr);

    data.push_back(arr);
    StreamInfo << data.dump();

    int a = data[0];
    double b = data[1];
    std::nullptr_t c = data[2];
    std::string d = data[3];
    bool e = data[4];
    bool f = data[5];
    StreamInfo << a << ", " << b << ", " << c << ", " << d << ", " << e << ", "
               << f;
  }

  {
    const Json js = Json::parse(
        R"({"test1":false,"test2":true,"test3":"string","test4":null})");
    StreamInfo << js["test1"].get<bool>() << ", " << js["test3"].get<std::string>();
  }

  return 0;
}