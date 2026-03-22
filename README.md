# ZcAILib

Qt-based AI client library for chat completion APIs.  
It currently provides:

- OpenAI / DeepSeek service switching
- model list fetching
- system prompt support
- non-streaming reply callbacks
- streaming reply callbacks based on SSE

## Requirements

- Qt 6
- CMake 3.16+
- C++17

This project links against:

- `Qt6::Core`
- `Qt6::Network`
- `Qt6::Widgets` for the example app

## Build

```bash
cmake -S . -B build
cmake --build build --config Debug
```

The example executable will be generated as:

- `build/Debug/ZcAiLib-Example.exe` on Windows with Visual Studio generators

## Add To Your Project

If you use this repository directly, the simplest way is:

```cmake
add_subdirectory(path/to/ZcAiLib)

target_link_libraries(YourApp PRIVATE
    ZcAiLib
    Qt6::Widgets
)
```

Then include:

```cpp
#include "AiProvider.h"
```

## Basic Usage

```cpp
AiProvider *ai = new AiProvider(this);

ai->setServiceType(AiProvider::DeepSeek);
ai->setApiKey("YOUR_API_KEY");
ai->setModel("deepseek-chat");
ai->setSystemPrompt("You are a helpful assistant.");

connect(ai, &AiProvider::replyReceived, this, [](const QString &reply) {
    qDebug() << "AI reply:" << reply;
});

connect(ai, &AiProvider::errorOccurred, this, [](const QString &error) {
    qWarning() << "AI error:" << error;
});

ai->chat("Hello");
```

## Streaming Usage

Streaming is disabled by default. Enable it explicitly before calling `chat()`.

```cpp
AiProvider *ai = new AiProvider(this);

ai->setServiceType(AiProvider::OpenAI);
ai->setApiKey("YOUR_API_KEY");
ai->setModel("gpt-3.5-turbo");
ai->setStreamEnabled(true);

connect(ai, &AiProvider::replyChunkReceived, this, [](const QString &chunk) {
    qDebug().noquote() << chunk;
});

connect(ai, &AiProvider::replyReceived, this, [](const QString &fullReply) {
    qDebug() << "stream finished:" << fullReply;
});

connect(ai, &AiProvider::errorOccurred, this, [](const QString &error) {
    qWarning() << error;
});

ai->chat("Write a short poem about Qt.");
```

### Streaming behavior

- `replyChunkReceived(...)` is emitted for every parsed content chunk
- `replyReceived(...)` is emitted once after the full streamed reply is complete
- `errorOccurred(...)` is emitted on HTTP or API errors

This means existing code that only listens to `replyReceived(...)` still works after streaming is added.

## Fetch Available Models

```cpp
connect(ai, &AiProvider::modelsReceived, this, [](const QList<AiProvider::ModelInfo> &models) {
    for (const auto &model : models) {
        qDebug() << model.id << model.ownedBy << model.created;
    }
});

ai->fetchModels();
```

`ModelInfo` contains:

- `id`
- `created`
- `ownedBy`
- `permissions`

## Service Configuration

Built-in service types:

- `AiProvider::OpenAI`
- `AiProvider::DeepSeek`
- `AiProvider::Custom`

For custom services:

```cpp
ai->setServiceType(AiProvider::Custom);
ai->setApiUrl("https://your-domain/v1/chat/completions");
ai->setApiKey("YOUR_API_KEY");
ai->setModel("your-model-id");
```

## Public API Summary

Main methods:

- `setServiceType(AiProvider::ServiceType type)`
- `setApiKey(const QString &apiKey)`
- `setApiUrl(const QString &url)`
- `setModel(const QString &model)`
- `setStreamEnabled(bool enabled)`
- `setSystemPrompt(const QString &prompt)`
- `fetchModels()`
- `chat(const QString &message)`

Main signals:

- `replyReceived(const QString &reply)`
- `replyChunkReceived(const QString &chunk)`
- `modelsReceived(const QList<ModelInfo> &models)`
- `errorOccurred(const QString &error)`

## Example

The example UI is in `example/mainwindow.cpp`.

It demonstrates:

- switching between OpenAI and DeepSeek
- fetching model lists from the API
- setting a system prompt
- enabling/disabling stream output
- rendering streamed output incrementally in a `QTextEdit`

## Notes

- Streaming support depends on the upstream API supporting SSE chat completion responses.
- The library currently targets chat-completions style APIs.
- `replyReceived(...)` remains the final completion signal for both streaming and non-streaming requests.
