/*
 * @Author: chenjingyu
 * @Date: 2026-02-27 17:58:27
 * @Contact: 2458006466@qq.com
 * @Description: ExampleBase64
 */
#include "Base/Base64.h"
#include "Base/Logger.h"

using namespace NAMESPACE;

int main(int argc, char *argv[]) {
  {
    std::string src = "Hello, MirrorYuChen!";
    std::string str_encoded = Base64::encode(
      reinterpret_cast<const unsigned char*>(src.data()), src.size()
    );
    // Expect "d2ZyZXN0IGh0dHAgZnJhbWV3b3Jr"
    StreamInfo << str_encoded;
    std::string str_decoded = Base64::decode(str_encoded);
    StreamInfo << str_decoded;
  }

  {
    std::string src = "123456,abcdEFG,_**_~@";
    std::string str_encoded = Base64::encode(
      reinterpret_cast<const unsigned char*>(src.data()), src.size()
    );
    std::string str_decoded = Base64::decode(str_encoded);
    StreamInfo << str_decoded;
  }
  return 0;
}