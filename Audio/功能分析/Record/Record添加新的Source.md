# AudioSource 添加指南 - SatCom 示例

# 需要添加: VOICE_SatCom_ECall = 12, VOICE_SatCom_Call = 13

  

## 概述

本文档记录了添加新 AudioSource 的完整修改清单，可作为未来添加新音频源的参考。

  

---

  

## 修改的文件清单

  

### 1. Java API - MediaRecorder.java

- **路径**: frameworks/base/media/java/android/media/MediaRecorder.java

  

**修改点**:

  

#### 1.1 添加常量定义 (约 Line 370)

```java

/**

 * Audio source for Satellite Communication (SatCom) emergency call.

 * @hide

 */

public static final int VOICE_SATCOM_ECALL = 12;

  

/**

 * Audio source for Satellite Communication (SatCom) voice call.

 * @hide

 */

public static final int VOICE_SATCOM_CALL = 13;

```

  

#### 1.2 更新 @Source 注解 (约 Line 428-441)

```java

@IntDef({

    AudioSource.DEFAULT,

    AudioSource.MIC,

    AudioSource.VOICE_UPLINK,

    AudioSource.VOICE_DOWNLINK,

    AudioSource.VOICE_CALL,

    AudioSource.CAMCORDER,

    AudioSource.VOICE_RECOGNITION,

    AudioSource.VOICE_COMMUNICATION,

    AudioSource.UNPROCESSED,

    AudioSource.VOICE_PERFORMANCE,

    AudioSource.VOICE_SATCOM_ECALL,    // 新增

    AudioSource.VOICE_SATCOM_CALL,     // 新增

})

```

  

#### 1.3 更新 @SystemSource 注解 (约 Line 444-462)

```java

@IntDef({

    // ... 同样的值 ...

    AudioSource.VOICE_SATCOM_ECALL,    // 新增

    AudioSource.VOICE_SATCOM_CALL,     // 新增

    // ... 其他值 ...

})

```

  

#### 1.4 更新 isSystemOnlyAudioSource() 方法 (约 Line 470-487)

```java

case AudioSource.VOICE_SATCOM_ECALL:

case AudioSource.VOICE_SATCOM_CALL:

    return false;  // 公开给所有应用

```

  

#### 1.5 更新 isValidAudioSource() 方法 (约 Line 494-514)

```java

case AudioSource.VOICE_SATCOM_ECALL:

case AudioSource.VOICE_SATCOM_CALL:

    return true;

```

  

#### 1.6 更新 toLogFriendlyAudioSource() 方法 (约 Line 517-554)

```java

case AudioSource.VOICE_SATCOM_ECALL:

    return "VOICE_SATCOM_ECALL";

case AudioSource.VOICE_SATCOM_CALL:

    return "VOICE_SATCOM_CALL";

```

  
  

### 2. AIDL 接口定义

- **路径**: system/hardware/interfaces/media/aidl/android/media/audio/common/AudioSource.aidl

  

**修改点** (约 Line 70-79):

```aidl

VOICE_PERFORMANCE = 10,

/** Audio source for Satellite Communication (SatCom) emergency call. */

VOICE_SATCOM_ECALL = 12,

/** Audio source for Satellite Communication (SatCom) voice call. */

VOICE_SATCOM_CALL = 13,

```

- **路径**: system/hardware/interfaces/media/aidl_api/android.media.audio.common.types/current/android/media/audio/common

执行 make update-api 会出现报错我们需要在对应的版本的AudioSouce.aidl再次添加并且执行({ find ./ -name "*.aidl" -print0 | LC_ALL=C sort -z | xargs -0 sha1sum && echo 3; } | sha1sum | cut -d " " -f 1)

把生成的hash码填写到.hash文件中

  

- **路径**: system/hardware/interfaces/media/Android.bp

frozen: false->frozen: true  

 make update-api 编译通过在改回来

  

### 3. Native C 头文件 - audio-hal-enums.h

- **路径**: system/media/audio/include/system/audio-hal-enums.h

  

**修改点** (约 Line 718-733):

```cpp

#define AUDIO_SOURCE_LIST_NO_SYS_DEF(V) \

    V(AUDIO_SOURCE_DEFAULT, 0) \

    // ...

    V(AUDIO_SOURCE_VOICE_PERFORMANCE, 10) \

    V(AUDIO_SOURCE_VOICE_SATCOM_ECALL, 12) \    // 新增

    V(AUDIO_SOURCE_VOICE_SATCOM_CALL, 13) \     // 新增

    V(AUDIO_SOURCE_ECHO_REFERENCE, 1997) \

    // ...

```

  
  

### 4. Audio Source 验证函数 - audio.h

- **路径**: system/media/audio/include/system/audio.h

  

**修改点** (约 Line 2110-2133):

```cpp

static inline bool audio_is_valid_audio_source(audio_source_t audioSource)

{

    switch (audioSource) {

    // ...

    case AUDIO_SOURCE_VOICE_PERFORMANCE:

    case AUDIO_SOURCE_VOICE_SATCOM_ECALL:    // 新增

    case AUDIO_SOURCE_VOICE_SATCOM_CALL:     // 新增

    case AUDIO_SOURCE_ECHO_REFERENCE:

    // ...

        return true;

    // ...

    }

}

```

  
  

### 5. C++ AIDL 转换层

- **路径**: frameworks/av/media/audioaidlconversion/AidlConversionCppNdk.cpp

  

**修改点 1** - aidl2legacy 转换 (约 Line 1730):

```cpp

case AudioSource::VOICE_PERFORMANCE:

    return AUDIO_SOURCE_VOICE_PERFORMANCE;

case AudioSource::VOICE_SATCOM_ECALL:        // 新增

    return AUDIO_SOURCE_VOICE_SATCOM_ECALL;

case AudioSource::VOICE_SATCOM_CALL:         // 新增

    return AUDIO_SOURCE_VOICE_SATCOM_CALL;

case AudioSource::ULTRASOUND:

```

  

**修改点 2** - legacy2aidl 转换 (约 Line 1769):

```cpp

case AUDIO_SOURCE_VOICE_PERFORMANCE:

    return AudioSource::VOICE_PERFORMANCE;

case AUDIO_SOURCE_VOICE_SATCOM_ECALL:        // 新增

    return AudioSource::VOICE_SATCOM_ECALL;

case AUDIO_SOURCE_VOICE_SATCOM_CALL:         // 新增

    return AudioSource::VOICE_SATCOM_CALL;

case AUDIO_SOURCE_ULTRASOUND:

```

  
  

### 6. AudioPolicy Engine - Engine.cpp

- **路径**: frameworks/av/services/audiopolicy/enginedefault/src/Engine.cpp

  

**修改点 1** - 通话时重定向 (约 Line 647-660):

```cpp

if (isInCall()) {

    switch (inputSource) {

    // ...

    case AUDIO_SOURCE_VOICE_PERFORMANCE:

    case AUDIO_SOURCE_VOICE_SATCOM_ECALL:    // 新增

    case AUDIO_SOURCE_VOICE_SATCOM_CALL:     // 新增

    case AUDIO_SOURCE_ULTRASOUND:

        inputSource = AUDIO_SOURCE_VOICE_COMMUNICATION;

        break;

    }

}

```

  

**修改点 2** - 设备路由 (约 Line 816-825):

```cpp

case AUDIO_SOURCE_VOICE_PERFORMANCE:

    // ...

    break;

case AUDIO_SOURCE_VOICE_SATCOM_ECALL:        // 新增

case AUDIO_SOURCE_VOICE_SATCOM_CALL:         // 新增

    // Use default input device for SatCom calls

    device = getIPDevice(availableDevices);

    if (device != nullptr) break;

    device = availableDevices.getFirstExistingDevice({

            AUDIO_DEVICE_IN_BUILTIN_MIC, AUDIO_DEVICE_IN_WIRED_HEADSET,

            AUDIO_DEVICE_IN_USB_HEADSET, AUDIO_DEVICE_IN_USB_DEVICE});

    break;

case AUDIO_SOURCE_REMOTE_SUBMIX:

```

  
  

### 7. Source 优先级 - policy.h


- **路径**: frameworks/av/services/audiopolicy/common/include/policy.h

  

**修改点** (约 Line 211-238):

```cpp

static inline int32_t source_priority(audio_source_t inputSource)

{

    switch (inputSource) {

    case AUDIO_SOURCE_VOICE_COMMUNICATION:

        return 10;

    // ...

    case AUDIO_SOURCE_VOICE_PERFORMANCE:

        return 8;

    case AUDIO_SOURCE_VOICE_SATCOM_ECALL:    // 新增

    case AUDIO_SOURCE_VOICE_SATCOM_CALL:     // 新增

        return 8;

    // ...

    }

}

```

### 8. AudioPolicyInterfaceImpl.cpp
不添加编译会报错
```cpp
	error::BinderResult<bool> AudioPolicyService::AudioPolicyClient::checkPermissionForInput(
        const AttributionSourceState& attrSource, const PermissionReqs& req) {
        switch (req.source) {
         case AudioSource::VOICE_SATCOM_ECALL:
        case AudioSource::VOICE_SATCOM_CALL:
        }
    }
```
### 9.AudioPolicyService.cpp
```cpp
bool AudioPolicyService::isVirtualSource(audio_source_t source)
{
    switch (source) {
        case AUDIO_SOURCE_VOICE_SATCOM_ECALL:
        case AUDIO_SOURCE_VOICE_SATCOM_CALL:
        }
}
```
---

  

## 未来添加新 AudioSource 的检查清单

  

### 核心定义层 (必须修改)

- [ ] MediaRecorder.java - AudioSource 内部类常量

- [ ] MediaRecorder.java - @Source IntDef 注解

- [ ] MediaRecorder.java - @SystemSource IntDef 注解

- [ ] MediaRecorder.java - isSystemOnlyAudioSource() 方法

- [ ] MediaRecorder.java - isValidAudioSource() 方法

- [ ] MediaRecorder.java - toLogFriendlyAudioSource() 方法

- [ ] AudioSource.aidl - 枚举定义

- [ ] audio-hal-enums.h - AUDIO_SOURCE_LIST_NO_SYS_DEF 宏

- [ ] audio.h - audio_is_valid_audio_source() 函数

  

### 转换层 (必须修改)

- [ ] AidlConversionCppNdk.cpp - aidl2legacy 转换

- [ ] AidlConversionCppNdk.cpp - legacy2aidl 转换

  

### AudioPolicy 引擎 (必须修改)

- [ ] Engine.cpp - 通话时重定向逻辑

- [ ] Engine.cpp - 设备路由逻辑

- [ ] policy.h - source_priority() 函数

  

### 可选/检查项

- [ ] 检查其他转换文件 (AidlConversionNdk.cpp, AidlConversion.cpp)

- [ ] AudioDeviceInventory.java - CAPTURE_PRESETS

- [ ] ServiceUtilities.cpp - 权限检查

- [ ] enums.proto - Proto 日志枚举

- [ ] audio_policy_configuration.xsd - XML 配置枚举

- [ ] Qualcomm XSD (如使用)

  

---

  

## 注意事项

  

1. **权限需求**: 新 source 需要特殊权限吗？

2. **系统独占**: isSystemOnlyAudioSource() 应该返回 true 还是 false？

3. **设备路由**: 需要为新 source 指定默认音频设备吗？

4. **优先级**: source_priority() 中应该设置多少优先级？