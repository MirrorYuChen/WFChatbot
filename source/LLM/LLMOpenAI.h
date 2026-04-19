/*
 * @Author: chenjingyu
 * @Date: 2026-04-18 21:22:29
 * @Contact: 2458006466@qq.com
 * @Description: LLMOpenAI
 */
#pragma once

#include "Api.h"

#include "Base/Json.h"
#include "LLM/LLMBase.h"

#include <workflow/HttpMessage.h>
#include <workflow/WFTaskFactory.h>

#include <string>
#include <vector>

NAMESPACE_BEGIN
class API LLMOpenAI : public LLMBase {
public:
  explicit LLMOpenAI(const Json &cfg);
  ~LLMOpenAI() override = default;

  bool support_multimodal_input() const override { return support_multimodal_; }

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
  bool support_multimodal_{false};
};

NAMESPACE_END
