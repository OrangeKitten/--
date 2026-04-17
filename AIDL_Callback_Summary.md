# AIDL 回调注册问题总结

  

## 问题背景

  

编译报错：`GxaAidlCallback` 是抽象类，无法实例化。

  

```

error: allocating an object of abstract class type 'com::gxa::car::audio::GxaAidlCallback'

  unimplemented pure virtual method 'asBinder'

  unimplemented pure virtual method 'isRemote'

```

  

## 修复过程

  

### 第一步：补齐纯虚方法

  

最初尝试直接在 `GxaAidlCallback` 中实现 `asBinder()` 和 `isRemote()`：

  

```cpp

// 头文件

::ndk::SpAIBinder asBinder() override;

bool isRemote() override;

  

// cpp 实现

::ndk::SpAIBinder GxaAidlCallback::asBinder() {

    return ::ndk::SpAIBinder();  // ❌ 空 binder

}

bool GxaAidlCallback::isRemote() {

    return false;

}

```

  

结果：编译通过，但服务端收到的是**空指针**。

  

### 第二步：继承 `IAudioControlCallbackDefault`

  

尝试继承 AIDL 生成的默认实现类：

  

```cpp

class GxaAidlCallback : public IAudioControlCallbackDefault { ... }

```

  

结果：`asBinder()` 依然返回空 `SpAIBinder()`，服务端仍然收到空引用。

  

### 第三步：继承 `BnAudioControlCallback`（最终方案）

  

```cpp

// 头文件

#include <aidl/vendor/idc/hardware/dsp/BnAudioControlCallback.h>

  

class GxaAidlCallback : public ::aidl::vendor::idc::hardware::dsp::BnAudioControlCallback {

public:

    GxaAidlCallback(sp<ICarAudioHalCallback> callback)

        : pCallback(callback) {}

  

    // 只需实现业务方法

    ::ndk::ScopedAStatus onReceiveSourceRequester(...) override;

    ::ndk::ScopedAStatus onReceiveEvent(...) override;

    ::ndk::ScopedAStatus onSetParameters(...) override;

  

private:

    sp<ICarAudioHalCallback> pCallback;

};

```

  

结果：✅ 服务端可以正常收到有效的 callback 引用。

  

## 核心原理：

  
  

```

它的设计目的是作为**服务端占位 stub**，不是用来传递给其他服务的有效 callback。

  

## 为什么 `BnAudioControlCallback` 可以

  

```cpp

// BnAudioControlCallback.cpp

::ndk::SpAIBinder BnAudioControlCallback::createBinder() {

    AIBinder* binder = AIBinder_new(

        _g_aidl_vendor_idc_hardware_dsp_IAudioControlCallback_clazz,

        static_cast<void*>(this)

    );

    return ::ndk::SpAIBinder(binder);

}

```

  

- `AIBinder_new()` 创建真正的 binder，注册到 ServiceManager

- 服务端可以通过 binder 找到并调用你的回调方法

  

## 总结

  

> H只有继承 `BnAudioControlCallback` 才能创建有效的 `AIBinder`，让服务端正常调用回调方法。

  

## 修改文件清单

  

| 文件 | 修改内容 |

|------|---------|

| `GxaAidlInterface.h` | 添加 `#include BnAudioControlCallback.h`，继承改为 `BnAudioControlCallback`，移除冗余方法声明 |

| `GxaAidlInterface.cpp` | 移除 `getInterfaceVersion`、`getInterfaceHash`、`asBinder`、`isRemote` 的实现 |