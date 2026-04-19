/*
 * @Author: chenjingyu
 * @Date: 2026-04-18 21:03:04
 * @Contact: 2458006466@qq.com
 * @Description: LLMDeepSeek
 */
#pragma once

#include "Api.h"
#include "Base/Json.h"
#include "LLM/LLMBase.h"

#include <string>
#include <vector>
#include <workflow/HttpMessage.h>
#include <workflow/WFTaskFactory.h>

NAMESPACE_BEGIN
class API LLMDeepSeek : public LLMBase {
public:
  explicit LLMDeepSeek(const Json &cfg);
  ~LLMDeepSeek() override = default;

  bool support_multimodal_input() const override { return false; }

protected:
  void ChatImpl(const std::vector<Message> &messages, const Json &functions,
                const GenerateConfig &config, LLMStreamCallback stream_cb,
                LLMCompleteCallback complete_cb,
                LLMErrorCallback error_cb) override;

private:
  void HttpCallback(WFHttpTask *task, LLMStreamCallback stream_cb,
                    LLMCompleteCallback complete_cb, LLMErrorCallback error_cb,
                    bool stream, std::vector<Message> *messages);

private:
  std::string api_url_;
};
NAMESPACE_END
