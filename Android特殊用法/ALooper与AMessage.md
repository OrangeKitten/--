# ALooper与AMessage详解与测试用例

## 📋 概述

在Android的多媒体框架中，`ALooper`和`AMessage`是用于实现异步消息处理的核心组件：

- **🔄 ALooper**：事件循环类，负责管理消息队列并在独立线程中处理消息
- **📨 AMessage**：消息载体类，包含消息类型和数据参数
- **🎯 AHandler**：消息处理器基类，定义消息处理逻辑

## 🏗️ 架构设计

```
┌─────────────────┐    post()    ┌─────────────────┐    deliver()   ┌─────────────────┐
│    AMessage     │──────────────>│     ALooper     │──────────────>│    AHandler     │
│                 │              │                 │               │                 │
│ - what (type)   │              │ - MessageQueue  │               │ - onMessageRcv  │
│ - data params   │              │ - Thread        │               │ - Custom Logic  │
│ - target        │              │ - Timer         │               │                 │
└─────────────────┘              └─────────────────┘               └─────────────────┘
```

## 🔧 核心API介绍

### ALooper 主要方法

```cpp
class ALooper : public RefBase {
public:
    // 启动消息循环（在新线程中运行）
    status_t start(bool runOnCallingThread = false, 
                   bool canCallJava = false, 
                   int32_t priority = PRIORITY_DEFAULT);
    
    // 停止消息循环
    status_t stop();
    
    // 注册消息处理器
    handler_id registerHandler(const sp<AHandler> &handler);
    
    // 注销消息处理器
    void unregisterHandler(handler_id handlerID);
    
    // 设置循环器名称
    void setName(const char *name);
    
    // 获取当前时间（微秒）
    static int64_t GetNowUs();
};
```

### AMessage 主要方法

```cpp
class AMessage : public RefBase {
public:
    // 构造函数
    AMessage(uint32_t what, const sp<AHandler> &handler);
    
    // 设置各种类型的参数
    void setInt32(const char *name, int32_t value);
    void setInt64(const char *name, int64_t value);
    void setSize(const char *name, size_t value);
    void setFloat(const char *name, float value);
    void setDouble(const char *name, double value);
    void setString(const char *name, const char *s);
    void setString(const char *name, const AString &s);
    void setPointer(const char *name, void *value);
    void setBuffer(const char *name, const sp<ABuffer> &buffer);
    void setMessage(const char *name, const sp<AMessage> &obj);
    
    // 获取各种类型的参数
    bool findInt32(const char *name, int32_t *value) const;
    bool findInt64(const char *name, int64_t *value) const;
    bool findSize(const char *name, size_t *value) const;
    bool findFloat(const char *name, float *value) const;
    bool findDouble(const char *name, double *value) const;
    bool findString(const char *name, AString *value) const;
    bool findPointer(const char *name, void **value) const;
    bool findBuffer(const char *name, sp<ABuffer> *buffer) const;
    bool findMessage(const char *name, sp<AMessage> *obj) const;
    
    // 发送消息
    void post(int64_t delayUs = 0);
    
    // 发送消息并等待响应
    status_t postAndAwaitResponse(sp<AMessage> *response);
    
    // 消息类型
    uint32_t what() const { return mWhat; }
};
```

### AHandler 主要方法

```cpp
class AHandler : public RefBase {
public:
    AHandler() : mID(0) {}
    
    // 核心消息处理方法（需要重写）
    virtual void onMessageReceived(const sp<AMessage> &msg) = 0;
    
    // 获取处理器ID
    ALooper::handler_id id() const { return mID; }
    
protected:
    virtual ~AHandler() {}
    
private:
    friend struct ALooper;
    ALooper::handler_id mID;
};
```

## 📝 基础测试用例

### 1. 简单消息发送和处理

```cpp
#include <media/stagefright/foundation/ALooper.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/foundation/AHandler.h>
#include <utils/Log.h>
#include <unistd.h>

using namespace android;

// 自定义消息处理器
class SimpleHandler : public AHandler {
public:
    enum {
        kWhatHello = 'helo',
        kWhatGoodbye = 'gdby',
        kWhatCalculate = 'calc'
    };

protected:
    virtual void onMessageReceived(const sp<AMessage> &msg) override {
        switch (msg->what()) {
            case kWhatHello: {
                AString name;
                if (msg->findString("name", &name)) {
                    ALOGD("Hello %s!", name.c_str());
                } else {
                    ALOGD("Hello World!");
                }
                break;
            }
            case kWhatGoodbye: {
                ALOGD("Goodbye!");
                break;
            }
            case kWhatCalculate: {
                int32_t a, b;
                if (msg->findInt32("a", &a) && msg->findInt32("b", &b)) {
                    int32_t result = a + b;
                    ALOGD("Calculate: %d + %d = %d", a, b, result);
                    
                    // 发送结果消息
                    sp<AMessage> response = new AMessage(kWhatHello, this);
                    response->setString("name", "Calculator");
                    response->post(1000000); // 1秒后发送
                }
                break;
            }
            default:
                ALOGD("Unknown message: %c%c%c%c", 
                      (msg->what() >> 24) & 0xff,
                      (msg->what() >> 16) & 0xff, 
                      (msg->what() >> 8) & 0xff,
                      msg->what() & 0xff);
                break;
        }
    }
};

void testBasicMessage() {
    ALOGD("=== Basic Message Test ===");
    
    // 1. 创建并启动ALooper
    sp<ALooper> looper = new ALooper();
    looper->setName("BasicTestLooper");
    looper->start();
    
    // 2. 创建并注册处理器
    sp<SimpleHandler> handler = new SimpleHandler();
    looper->registerHandler(handler);
    
    // 3. 发送简单消息
    sp<AMessage> helloMsg = new AMessage(SimpleHandler::kWhatHello, handler);
    helloMsg->post();
    
    // 4. 发送带参数的消息
    sp<AMessage> namedHelloMsg = new AMessage(SimpleHandler::kWhatHello, handler);
    namedHelloMsg->setString("name", "Android Developer");
    namedHelloMsg->post(500000); // 0.5秒延迟
    
    // 5. 发送计算消息
    sp<AMessage> calcMsg = new AMessage(SimpleHandler::kWhatCalculate, handler);
    calcMsg->setInt32("a", 10);
    calcMsg->setInt32("b", 20);
    calcMsg->post(1000000); // 1秒延迟
    
    // 6. 发送再见消息
    sp<AMessage> goodbyeMsg = new AMessage(SimpleHandler::kWhatGoodbye, handler);
    goodbyeMsg->post(3000000); // 3秒延迟
    
    // 等待消息处理完成
    sleep(5);
    
    // 7. 清理
    looper->unregisterHandler(handler->id());
    looper->stop();
    
    ALOGD("=== Basic Message Test Completed ===");
}
```

### 2. 高级消息处理（请求-响应模式）

```cpp
class AdvancedHandler : public AHandler {
public:
    enum {
        kWhatRequest = 'rqst',
        kWhatResponse = 'resp',
        kWhatAsyncTask = 'task'
    };

protected:
    virtual void onMessageReceived(const sp<AMessage> &msg) override {
        switch (msg->what()) {
            case kWhatRequest: {
                AString operation;
                if (msg->findString("operation", &operation)) {
                    ALOGD("Processing request: %s", operation.c_str());
                    
                    // 模拟异步处理
                    sp<AMessage> asyncMsg = new AMessage(kWhatAsyncTask, this);
                    asyncMsg->setString("operation", operation);
                    asyncMsg->setMessage("replyTo", msg);
                    asyncMsg->post(1000000); // 1秒后处理
                }
                break;
            }
            case kWhatAsyncTask: {
                AString operation;
                sp<AMessage> replyTo;
                if (msg->findString("operation", &operation) && 
                    msg->findMessage("replyTo", &replyTo)) {
                    
                    // 模拟处理时间
                    ALOGD("Executing: %s", operation.c_str());
                    
                    // 发送响应
                    sp<AMessage> response = new AMessage(kWhatResponse, this);
                    response->setString("result", "Success");
                    response->setString("operation", operation);
                    response->post();
                }
                break;
            }
            case kWhatResponse: {
                AString result, operation;
                if (msg->findString("result", &result) && 
                    msg->findString("operation", &operation)) {
                    ALOGD("Response received - Operation: %s, Result: %s", 
                          operation.c_str(), result.c_str());
                }
                break;
            }
        }
    }
};

void testAdvancedMessage() {
    ALOGD("=== Advanced Message Test ===");
    
    sp<ALooper> looper = new ALooper();
    looper->setName("AdvancedTestLooper");
    looper->start();
    
    sp<AdvancedHandler> handler = new AdvancedHandler();
    looper->registerHandler(handler);
    
    // 发送请求消息
    sp<AMessage> request = new AMessage(AdvancedHandler::kWhatRequest, handler);
    request->setString("operation", "DataProcessing");
    request->post();
    
    sleep(3);
    
    looper->unregisterHandler(handler->id());
    looper->stop();
    
    ALOGD("=== Advanced Message Test Completed ===");
}
```

### 3. 多处理器协作测试

```cpp
class ProducerHandler : public AHandler {
public:
    enum { kWhatProduce = 'prod' };
    
    ProducerHandler(const sp<AHandler>& consumer) : mConsumer(consumer) {}

protected:
    virtual void onMessageReceived(const sp<AMessage> &msg) override {
        switch (msg->what()) {
            case kWhatProduce: {
                int32_t data = rand() % 100;
                ALOGD("Producer: Generated data %d", data);
                
                // 发送给消费者
                sp<AMessage> consumeMsg = new AMessage('cons', mConsumer);
                consumeMsg->setInt32("data", data);
                consumeMsg->post();
                
                // 继续生产（每2秒一次）
                sp<AMessage> nextProduce = new AMessage(kWhatProduce, this);
                nextProduce->post(2000000);
                break;
            }
        }
    }

private:
    sp<AHandler> mConsumer;
};

class ConsumerHandler : public AHandler {
public:
    ConsumerHandler() : mTotalData(0), mCount(0) {}

protected:
    virtual void onMessageReceived(const sp<AMessage> &msg) override {
        switch (msg->what()) {
            case 'cons': {
                int32_t data;
                if (msg->findInt32("data", &data)) {
                    mTotalData += data;
                    mCount++;
                    ALOGD("Consumer: Received data %d, Total: %d, Count: %d", 
                          data, mTotalData, mCount);
                }
                break;
            }
        }
    }

private:
    int32_t mTotalData;
    int32_t mCount;
};

void testMultiHandler() {
    ALOGD("=== Multi-Handler Test ===");
    
    sp<ALooper> looper = new ALooper();
    looper->setName("MultiHandlerLooper");
    looper->start();
    
    sp<ConsumerHandler> consumer = new ConsumerHandler();
    sp<ProducerHandler> producer = new ProducerHandler(consumer);
    
    looper->registerHandler(consumer);
    looper->registerHandler(producer);
    
    // 启动生产
    sp<AMessage> startMsg = new AMessage(ProducerHandler::kWhatProduce, producer);
    startMsg->post();
    
    sleep(10); // 运行10秒
    
    looper->unregisterHandler(consumer->id());
    looper->unregisterHandler(producer->id());
    looper->stop();
    
    ALOGD("=== Multi-Handler Test Completed ===");
}
```

### 4. 数据类型测试

```cpp
class DataTypeHandler : public AHandler {
public:
    enum { kWhatTestDataTypes = 'data' };

protected:
    virtual void onMessageReceived(const sp<AMessage> &msg) override {
        switch (msg->what()) {
            case kWhatTestDataTypes: {
                // 测试各种数据类型
                int32_t intVal;
                int64_t longVal;
                float floatVal;
                double doubleVal;
                AString stringVal;
                void* ptrVal;
                
                if (msg->findInt32("int_param", &intVal)) {
                    ALOGD("Int32: %d", intVal);
                }
                
                if (msg->findInt64("long_param", &longVal)) {
                    ALOGD("Int64: %lld", (long long)longVal);
                }
                
                if (msg->findFloat("float_param", &floatVal)) {
                    ALOGD("Float: %f", floatVal);
                }
                
                if (msg->findDouble("double_param", &doubleVal)) {
                    ALOGD("Double: %f", doubleVal);
                }
                
                if (msg->findString("string_param", &stringVal)) {
                    ALOGD("String: %s", stringVal.c_str());
                }
                
                if (msg->findPointer("ptr_param", &ptrVal)) {
                    ALOGD("Pointer: %p", ptrVal);
                }
                break;
            }
        }
    }
};

void testDataTypes() {
    ALOGD("=== Data Types Test ===");
    
    sp<ALooper> looper = new ALooper();
    looper->setName("DataTypeLooper");
    looper->start();
    
    sp<DataTypeHandler> handler = new DataTypeHandler();
    looper->registerHandler(handler);
    
    // 创建包含各种数据类型的消息
    sp<AMessage> dataMsg = new AMessage(DataTypeHandler::kWhatTestDataTypes, handler);
    dataMsg->setInt32("int_param", 42);
    dataMsg->setInt64("long_param", 9876543210LL);
    dataMsg->setFloat("float_param", 3.14f);
    dataMsg->setDouble("double_param", 2.718281828);
    dataMsg->setString("string_param", "Hello AMessage!");
    
    int testData = 100;
    dataMsg->setPointer("ptr_param", &testData);
    
    dataMsg->post();
    
    sleep(2);
    
    looper->unregisterHandler(handler->id());
    looper->stop();
    
    ALOGD("=== Data Types Test Completed ===");
}
```

## 🔍 实际项目中的使用示例

### 音频控制中的应用

```cpp
// 基于代码库中的实际例子
class AudioControlHandler : public MessageHandler {
public:
    void handleMessage(const Message& msg) override {
        switch (msg.what) {
            case MSG_SET_MEDIA_PATH_STEREO:
                ALOGD("Setting media path to stereo");
                // 调用实际的音频路径设置
                break;
            case MSG_SET_MEDIA_SOURCE_MEDIA:
                ALOGD("Setting media source to media");
                // 调用实际的音频源设置
                break;
            default:
                ALOGD("Unknown audio control message: %d", msg.what);
                break;
        }
    }
};

class AudioControlLooper {
private:
    sp<Looper> mLooper;
    std::thread mThread;
    std::atomic<bool> mStopThread;

public:
    AudioControlLooper() {
        mLooper = new Looper(false);
        mStopThread = false;
        mThread = std::thread(&AudioControlLooper::threadLoop, this);
    }
    
    void threadLoop() {
        Looper::setForThread(mLooper);
        while (!mStopThread.load()) {
            mLooper->pollOnce(200); // 200ms超时
        }
    }
    
    void setMediaPath(int path) {
        sp<AudioControlHandler> handler = new AudioControlHandler();
        mLooper->sendMessage(handler, path);
    }
};
```

## 📚 最佳实践与注意事项

### ✅ 最佳实践

1. **合理的消息类型定义**
   ```cpp
   enum {
       kWhatStart = 'strt',
       kWhatStop  = 'stop',
       kWhatPause = 'paus'
   };
   ```

2. **资源管理**
   ```cpp
   // 总是在适当的时候停止looper
   looper->stop();
   // 注销处理器
   looper->unregisterHandler(handlerId);
   ```

3. **错误处理**
   ```cpp
   status_t result = looper->start();
   if (result != OK) {
       ALOGE("Failed to start looper: %d", result);
       return;
   }
   ```

### ⚠️ 注意事项

1. **线程安全**：ALooper在独立线程中运行，注意数据访问的线程安全
2. **内存管理**：使用智能指针(sp)管理对象生命周期
3. **消息队列**：避免发送过多消息导致队列积压
4. **延迟时间**：延迟时间单位是微秒(μs)，不是毫秒

## 🧪 完整测试程序

```cpp
#include <media/stagefright/foundation/ALooper.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/foundation/AHandler.h>
#include <utils/Log.h>
#include <unistd.h>

using namespace android;

int main() {
    ALOGD("Starting ALooper & AMessage Tests...");
    
    // 运行各种测试
    testBasicMessage();
    sleep(1);
    
    testAdvancedMessage();
    sleep(1);
    
    testMultiHandler();
    sleep(1);
    
    testDataTypes();
    
    ALOGD("All tests completed!");
    return 0;
}
```

## 🔗 相关资源

- Android Stagefright框架文档
- AOSP源码：`frameworks/av/media/libstagefright/foundation/`
- Android NDK开发指南

---

*本文档提供了ALooper与AMessage的完整使用指南和测试用例，可以作为Android多媒体开发的参考资料。*
'


```mermaid
title Untitled

Alice->Bob: Authentication Request
note right of Bob: Bob thinks about it
Bob->Alice: Authentication Response
```