/*
 * @Author: chenjingyu
 * @Date: 2026-04-18 20:33:52
 * @Contact: 2458006466@qq.com
 * @Description: LLMDashScope
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
class API LLMDashScope : public LLMBase {
public:
  explicit LLMDashScope(const Json &cfg);
  ~LLMDashScope() override = default;

  bool support_multimodal_input() const override { return support_multimodal_; }

protected:
  void ChatImpl(const std::vector<Message> &messages, const Json &functions,
                const GenerateConfig &config, LLMStreamCallback stream_cb,
                LLMCompleteCallback complete_cb,
                LLMErrorCallback error_cb) override;

private:
  // HTTP callback for async response handling
  void HttpCallback(WFHttpTask *task, LLMStreamCallback stream_cb,
                    LLMCompleteCallback complete_cb, LLMErrorCallback error_cb,
                    bool stream, std::vector<Message> *messages);
private:
  std::string api_url_;
  bool support_multimodal_{false};
  bool enable_thinking_{false};
};

NAMESPACE_END
