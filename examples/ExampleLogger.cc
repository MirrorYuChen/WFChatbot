/*
 * @Author: chenjingyu
 * @Date: 2024-06-21 11:52:25
 * @Contact: 2458006466@qq.com
 * @Description: ExampleLogWrapper
 */
#include "Base/Logger.h"

int main(int argc, char *argv[]) {
  NAMESPACE::setLogger(NAMESPACE::CreateLogger("./log.txt", "mirror"));
  StreamInfo << "Hello, World.";
  FormatInfo("This is a Example for logger function.");
  for (int i = 0; i < 100; ++i) {
    FormatDebug("i: {}.", i);
  }
  FormatWarn("you can print double {}.", 5.4);

  StreamDebug << "Hello Yu.";
  StreamInfo << "{" << "Hello" << "}";

  PrintError("%d.", 10);
  PrintInfo("%f.", 432.0f);
  PrintDebug("SB.");

  FormatError("ni hao: {}.", "2b");

  std::string hello = "Hello, World!";
  FormatError(hello);
  PrintError(hello);

  CHECK_EQ(1, 2) << "CHECK Funny!";
  CHECK(1 == 2) << "Funny!";

  return 0;
}