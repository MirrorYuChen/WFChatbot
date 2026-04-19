# WFChatbot

* C++ Chatbot Framework 基于 [workflow](https://github.com/sogou/workflow) 高性能异步 HTTP 框架

## 1.项目描述

* 个人练手项目，仿照Qwen-Agent，实现的一个简易的Chatbot。一个高性能的 C++ Chatbot 服务框架，支持多 LLM 提供商、真流式响应、工具调用和上下文管理。

## 2.模块结构

```
WFChatbot/
├── source/
│   ├── Base/          # 基础工具库
│   │   ├── Logger     # 日志系统
│   │   ├── Json       # JSON 解析/序列化
│   │   ├── Base64     # Base64 编解码
│   │   ├── Timestamp  # 时间戳工具
│   │   └── AutoTimer  # 自动计时器
│   ├── LLM/           # LLM 客户端
│   │   ├── LLMBase    # 基础抽象类
│   │   ├── LLMOpenAI  # OpenAI API 支持
│   │   ├── LLMDeepSeek # DeepSeek API 支持
│   │   └── LLMDashScope # 阿里云 DashScope 支持
│   ├── Agent/         # Agent 框架
│   │   └── Agent      # 智能代理实现
│   ├── Server/        # 服务器
│   │   ├── ChatbotServer # 聊天服务
│   │   └── Callback   # HTTP 回调处理
│   ├── Tools/         # 工具集
│   │   ├── ToolBuildin # 内置工具
│   │   └── ToolAmapWeather # 高德天气工具
│   └── Util/          # 工具函数
│       ├── FileUtil   # 文件操作
│       └── PathUtil   # 路径操作
└── examples/          # 示例代码
    ├── ExampleChatbot # 聊天机器人示例
    ├── ExampleJson    # JSON 使用示例
    ├── ExampleBase64  # Base64 使用示例
    └── ExampleLogger  # 日志使用示例
```

## 3.核心特性

### 3.1. 真流式响应
- 使用 `WFHttpChunkedClient` 实现实时流式数据传输
- LLM 返回的每个 chunk 立即推送给客户端
- 服务端使用 `task->push()` 实时发送 SSE 数据

### 3.2. 多 LLM 支持
- **OpenAI**: GPT-4o, GPT-4 Vision, GPT-4 Turbo 等
- **DeepSeek**: DeepSeek Coder, DeepSeek Chat 等
- **DashScope**: Qwen, VL 等阿里云模型

### 3.3. 工具调用
- 支持自定义工具注册
- 内置工具：高德天气
- Agent 自动决策是否调用工具

### 3.4. 上下文管理
- 基于 Redis 的会话上下文存储
- 支持多用户并发
- 自动 trimming 过长上下文

## 4.编译构建

```bash
# 1.安装workflow
>> git clone https://github.com/sogou/workflow
>> cd workflow && make && sudo make install && sudo ldconfig

# 2.安装hiredis
## 2.1 apt安装
>> sudo apt-get -y install libhiredis-dev
## 2.2 源码安装
>> git clone https://github.com/redis/hiredis.git 
>> cd hiredis && make -j 16 && make install && sudo ldconfig

# 3.创建构建目录
>> mkdir build && cd build

# 4.配置并编译
>> cmake .. && make -j4
```

## 5.快速开始

```bash
# 1.设置环境变量
export OPENAI_API_KEY=your_api_key
export DEEPSEEK_API_KEY=your_api_key
export DASHSCOPE_API_KEY=your_api_key

# 2.启动一个redis
>> docker pull redis
>> docker run -d \
  --name my-redis \
  -p 6379:6379 \
  -v redis-data:/data \
  redis redis-server --appendonly yes

# 3.运行示例
>> ./build/examples/ExampleChatbot
```

## 6.API 接口

### 6.1 聊天完成

```bash
# 流式请求
curl -N -X POST http://localhost:8888/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{
    "user_id": "user123",
    "stream": true,
    "messages": [
      {"role": "user", "content": "你好"}
    ]
  }'

# 非流式请求
curl -X POST http://localhost:8888/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{
    "user_id": "user123",
    "stream": false,
    "messages": [
      {"role": "user", "content": "你好"}
    ]
  }'
```

### 6.2 其他接口

| 方法 | 路径 | 描述 |
|------|------|------|
| GET | /health | 健康检查 |
| GET | /v1/models | 获取可用模型 |
| GET | /v1/context/{user_id} | 获取用户上下文 |
| DELETE | /v1/context/{user_id} | 清除用户上下文 |

## 7.配置说明

可以通过配置文件或环境变量设置：

```json
{
  "llm_model_type": "openai",
  "llm_model": "gpt-4o",
  "llm_api_key": "${OPENAI_API_KEY}",
  "redis_url": "redis://localhost:6379",
  "server_port": 8888,
  "system_message": "你是一个有用的AI助手"
}
```

## 8.环境变量

| 变量名 | 描述 |
|--------|------|
| OPENAI_API_KEY | OpenAI API Key |
| DEEPSEEK_API_KEY | DeepSeek API Key |
| DASHSCOPE_API_KEY | 阿里云 DashScope API Key |

## 9.参考资料

- [workflow](https://github.com/sogou/workflow) - 高性能异步 HTTP 框架
- [GitHub Actions 快速入门](https://docs.github.com/zh/actions/get-started/quickstart)
- [wfrest](https://github.com/wfrest/wfrest)
- [workflow-ai](https://github.com/holmes1412/workflow-ai)
- [Qwen-Agent](https://github.com/QwenLM/Qwen-Agent)
