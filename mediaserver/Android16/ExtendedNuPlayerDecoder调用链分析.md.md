# 为什么最终调用到 `ExtendedNuPlayerDecoder::handleOutputFormatChange`

  

## 1. 背景

  

在 Android NuPlayer 框架中，高通（QTI）通过一套**插件化扩展机制**，在不修改 AOSP 源码的前提下，将关键类替换为自己的实现。

本文解释为什么明明代码写在 `NuPlayerDecoder.cpp` 的 `handleOutputFormatChange()` 里，实际运行时却跑到了 `libavenhancements.so` 中的 `ExtendedNuPlayerDecoder::handleOutputFormatChange()`。

  

---

  

## 2. 类继承关系

  

```

AVNuFactory                          (AOSP 基类工厂)

    └── ExtendedNuFactory            (高通扩展工厂，位于 libavenhancements.so)

  

NuPlayer::Decoder                    (AOSP 基类 Decoder)

    └── ExtendedNuPlayerDecoder      (高通扩展 Decoder，位于 libavenhancements.so)

```

  

关键点：`handleOutputFormatChange` 在基类中声明为 **`virtual`**：

  

```cpp

// NuPlayerDecoder.h 第125行

virtual void handleOutputFormatChange(const sp<AMessage> &format);

//  ↑ virtual！子类可以 override

```

  

高通子类 override 了它：

  

```cpp

// ExtendedNuPlayerDecoder.h 第42行

virtual void handleOutputFormatChange(const sp<AMessage> &format);

//  ↑ override 基类实现

```

  

---

  

## 3. 程序启动时的"偷梁换柱"

  

### 3.1 静态单例初始化流程

  

```

┌─────────────────────────────────────────────────────────────────┐

│                      程序启动（so 加载时）                        │

└─────────────────────────────────┬───────────────────────────────┘

                                  │

                                  ▼

┌─────────────────────────────────────────────────────────────────┐

│  AVNuFactory.cpp 第87~88行                                        │

│                                                                   │

│  AVNuFactory* AVNuFactory::sInst =                                │

│      ExtensionsLoader<AVNuFactory>::createInstance(               │

│          "createExtendedNuFactory"                                │

│      );                                                           │

└─────────────────────────────────┬───────────────────────────────┘

                                  │

                    ┌─────────────▼──────────────┐

                    │   ExtensionsLoader::loadLib()│

                    │   dlopen("libavenhancements  │

                    │           .so", RTLD_LAZY)   │

                    └─────────────┬──────────────┘

                                  │ 加载成功

                    ┌─────────────▼──────────────┐

                    │ ExtensionsLoader::           │

                    │   createInstance()           │

                    │ dlsym(handle,                │

                    │   "createExtendedNuFactory") │

                    └─────────────┬──────────────┘

                                  │ 找到符号

                    ┌─────────────▼──────────────┐

                    │ ExtendedNuFactory.cpp:38     │

                    │ AVNuFactory*                 │

                    │ createExtendedNuFactory() {  │

                    │     return new               │

                    │       ExtendedNuFactory;     │ ← 高通工厂对象

                    │ }                            │

                    └─────────────┬──────────────┘

                                  │

                                  ▼

              sInst = ExtendedNuFactory*

              ╔════════════════════════════════╗

              ║  披着 AVNuFactory 外衣          ║

              ║  实际是 ExtendedNuFactory 对象  ║

              ╚════════════════════════════════╝

```

  

---

  

## 4. 创建 Decoder 对象时的多态替换

  

### 4.1 时序图

  

```

NuPlayer.cpp                AVNuFactory(sInst)          ExtendedNuFactory

    │                             │                             │

    │  AVNuFactory::get()         │                             │

    │─────────────────────────────▶                             │

    │  返回 sInst                  │                             │

    │  (实际是 ExtendedNuFactory*) │                             │

    │◀─────────────────────────────                             │

    │                             │                             │

    │  createDecoder(...)         │                             │

    │─────────────────────────────▶  虚函数多态                  │

    │                             │─────────────────────────────▶

    │                             │  ExtendedNuFactory::         │

    │                             │  createDecoder()             │

    │                             │  return new                  │

    │                             │  ExtendedNuPlayerDecoder  ◀──┘

    │◀─────────────────────────────────────────────────────────

    │

    │  decoder 对象实际类型：ExtendedNuPlayerDecoder

```

  

### 4.2 关键代码对比

  

| 文件 | 代码 | 是否执行 |

|------|------|---------|

| `AVNuFactory.cpp:68` | `return new NuPlayer::Decoder(...)` | ❌ **不执行**（被虚函数覆盖） |

| `ExtendedNuFactory.cpp:53` | `return new ExtendedNuPlayerDecoder(...)` | ✅ **实际执行** |

  

---

  

## 5. `handleOutputFormatChange` 被调用时的多态

  

### 5.1 触发条件

  

`MediaCodec` 在解码过程中检测到音频输出格式变化（如采样率、声道数变化），触发回调：

  

### 5.2 完整调用时序图

  

```

MediaCodec(解码线程)        NuPlayerDecoder(ALooper线程)      ExtendedNuPlayerDecoder

      │                            │                                  │

      │ CB_OUTPUT_FORMAT_CHANGED   │                                  │

      │ ─────────────────────────▶ │                                  │

      │                            │                                  │

      │                    onMessageReceived()                        │

      │                    case CB_OUTPUT_FORMAT_CHANGED              │

      │                            │                                  │

      │                            │  handleOutputFormatChange(format)│

      │                            │  ← this 是 ExtendedNuPlayerDecoder*

      │                            │  虚函数查表                       │

      │                            │ ─────────────────────────────────▶

      │                            │                ExtendedNuPlayerDecoder::

      │                            │                handleOutputFormatChange()

      │                            │ ◀─────────────────────────────────

      │                            │                                  │

      │                            │  mRenderer->changeAudioFormat()  │

      │                            │ ──────────────────────────────────────────▶

      │                            │                              NuPlayerRenderer

```

  

### 5.3 虚函数多态原理

  

```

调用 handleOutputFormatChange(format)

         │

         │  this 指针 → ExtendedNuPlayerDecoder 对象

         │

         ▼

    查虚函数表（vtable）

         │

    ┌────┴────────────────────────────────────────────┐

    │             vtable of ExtendedNuPlayerDecoder    │

    │  handleOutputFormatChange → ExtendedNuPlayerDecoder::handleOutputFormatChange │

    └────────────────────────────────────────────────┘

         │

         ▼

    ExtendedNuPlayerDecoder::handleOutputFormatChange()  ✅

    NuPlayer::Decoder::handleOutputFormatChange()        ❌ 不执行

```

  

---

  

## 6. 完整调用链总览

  

```

┌──────────────────────────────────────────────────────────────────┐

│                     程序启动                                       │

│  AVNuFactory::sInst = dlopen(libavenhancements.so)               │

│                     + dlsym("createExtendedNuFactory")           │

│                     → sInst = new ExtendedNuFactory              │

└──────────────────────────────┬───────────────────────────────────┘

                               │

┌──────────────────────────────▼───────────────────────────────────┐

│                     创建 Decoder 对象                              │

│  NuPlayer.cpp:2127                                               │

│  AVNuFactory::get()->createDecoder()                             │

│    → get() 返回 ExtendedNuFactory*                               │

│    → 虚函数 → ExtendedNuFactory::createDecoder()                 │

│    → new ExtendedNuPlayerDecoder                                 │

└──────────────────────────────┬───────────────────────────────────┘

                               │

┌──────────────────────────────▼───────────────────────────────────┐

│                   音频格式变化（播放中）                             │

│  MediaCodec 回调 CB_OUTPUT_FORMAT_CHANGED                        │

│    → NuPlayerDecoder::onMessageReceived()                        │

│    → handleOutputFormatChange(format)                            │

│      （this = ExtendedNuPlayerDecoder*，虚函数多态）               │

│    → ExtendedNuPlayerDecoder::handleOutputFormatChange() ✅      │

└──────────────────────────────┬───────────────────────────────────┘

                               │

┌──────────────────────────────▼───────────────────────────────────┐

│                   通知 Renderer 重新打开 AudioSink                  │

│  mRenderer->changeAudioFormat(...)                               │

│    → NuPlayerRenderer::changeAudioFormat()                       │

│    → post kWhatChangeAudioFormat 消息                            │

│    → onChangeAudioFormat()                                       │

│    → onOpenAudioSink()                                           │

└──────────────────────────────────────────────────────────────────┘

```

  

---

  

## 7. 总结

  

高通使用了两层机制来实现"不改 AOSP 代码，替换关键实现"：

  

| 机制 | 作用 | 关键代码 |

|------|------|---------|

| **`dlopen` 动态加载** | 运行时加载 `libavenhancements.so` | `ExtensionsLoader::loadLib()` |

| **工厂模式** | 将单例 `sInst` 替换为 `ExtendedNuFactory` | `AVNuFactory::sInst = ...createExtendedNuFactory` |

| **C++ 虚函数多态** | 所有 `virtual` 函数调用走子类实现 | `virtual handleOutputFormatChange` |

  

> ⚠️ **注意**：如果你想修改 `handleOutputFormatChange` 的行为，必须修改：

> ```

> vendor/qcom/proprietary/commonsys/avenhancements/av/mediaplayerservice/ExtendedNuPlayerDecoder.cpp

> ```

> 修改 `NuPlayerDecoder.cpp` 中的基类实现**不会有任何效果**，因为该方法已被子类完全覆盖。