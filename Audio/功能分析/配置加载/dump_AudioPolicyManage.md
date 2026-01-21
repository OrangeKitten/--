# AudioPolicyManager Dump 文件结构总结

## 文件来源
这个 dump 是由 **AudioPolicyManager** 生成的，打印代码位于：
- **主文件**: `audiopolicy/managerdefault/AudioPolicyManager.cpp`
- **dump() 函数**: 当执行 `dumpsys media.audio_policy` 命令时调用

---

## Dump 文件包含的主要信息模块

### 1. **Supported System Usages**
- 系统支持的音频使用场景
- 包括：CALL_ASSISTANT, EMERGENCY, SAFETY, VEHICLE_STATUS, ANNOUNCEMENT

### 2. **UID Policy**
- UID 策略信息
- 包括：Assistants UIDs, Active Assistants UIDs, Accessibility UIDs
- Input Method Service UID
- RTT (Real-Time Text) 状态

### 3. **AudioCommandThread**
- 音频命令线程信息
- 地址：0xb400007a3ce09590
- 最近执行的命令记录

### 4. **OutputCommandThread**
- 输出命令线程信息
- 地址：0xb400007a3ce0b3d0
- 最近执行的命令记录

### 5. **AudioPolicyManager Dump** ⭐ (核心部分)
包含以下子模块：

#### 5.1 系统状态信息
- **Primary Output I/O handle**: 主输出句柄 (13)
- **Phone state**: 电话状态 (AUDIO_MODE_NORMAL)
- **Force use** 配置：
  - communications: 0
  - media: 10
  - record: 0
  - dock: 9
  - system: 0
  - HDMI system audio: 0
  - encoded surround output: 0
  - vibrate ringing: 0
- **TTS output**: 不可用
- **Master mono**: 关闭
- **Communication Strategy id**: 0
- **Config source**: AIDL HAL

#### 5.2 **Available output devices (8)** ⭐
当前可用的输出设备列表，每个设备包含：
- Port ID (系统分配的端口ID)
- 设备名称
- 设备类型 (AUDIO_DEVICE_OUT_*)
- 地址
- Encapsulation modes
- Port Hal ID (HAL层的端口ID)
- Profiles (音频配置文件)
  - 格式 (AUDIO_FORMAT_*)
  - 采样率
  - 声道掩码
  - 封装类型

**示例设备**：
1. speaker (Port ID: 2)
2. telephony_tx (Port ID: 29)
3. fm_out (Port ID: 3)
4. Auto TTS Bus (Port ID: 22)
5. Sys Notification Bus (Port ID: 19)
6. TTS VR Bus (Port ID: 16)
7. Nav Guidance Bus (Port ID: 13)
8. Media Bus (Port ID: 10)

#### 5.3 **Available input devices (7)** ⭐
当前可用的输入设备列表，结构类似输出设备：
- Port ID
- 设备名称
- 设备类型 (AUDIO_DEVICE_IN_*)
- 地址
- Port Hal ID
- Profiles

**示例设备**：
1. built_in_mic (Port ID: 36)
2. telephony_rx (Port ID: 37)
3. built_in_back_mic (Port ID: 38)
4. Remote Submix In (Port ID: 49)
5. fm_tuner_mic (Port ID: 39)
6. tuner_in
7. echo_reference_mic

#### 5.4 **Hardware modules (4)**
硬件模块信息，包括：
- 模块名称
- Handle (句柄)
- 支持的输出/输入 MixPorts
- 声明的设备 (Declared devices)
- 动态设备 (Dynamic devices)
- 路由信息 (Routes)

**典型模块**：
- primary (主模块)
- a2dp (蓝牙A2DP)
- usb (USB音频)
- r_submix (远程submix)

#### 5.5 **Outputs (12)**
当前打开的输出流信息，每个输出包含：
- I/O handle
- 采样率
- 格式
- 声道
- 延迟
- Flags (输出标志)
- 设备
- 活动的 Tracks
- 音量信息
- Effect chains

#### 5.6 **Inputs (0)**
当前打开的输入流信息（本例中为0）

#### 5.7 **Total Effects**
音效统计信息：
- CPU 使用: 0.000000 MIPS
- 内存使用: 0 KB
- 最大内存: 0 KB

#### 5.8 **Audio Patches (12)**
音频补丁（路由连接）信息：
- Patch handle
- Source ports (源端口)
- Sink ports (目标端口)

#### 5.9 **Audio Policy Mix**
音频策略混音信息：
- Mix 配置
- 路由规则
- 设备选择策略

#### 5.10 **Audio sources (0)**
音频源信息（本例中为0）

#### 5.11 **AllowedCapturePolicies**
允许的捕获策略

#### 5.12 **Preferred mixer audio configuration**
首选混音器音频配置

### 6. **Policy Engine dump** ⭐
策略引擎信息：

#### 6.1 **Product Strategies dump**
产品策略转储：
- 各种音频使用场景的路由策略
- 设备选择规则

#### 6.2 **Device role per product strategy dump**
每个产品策略的设备角色

#### 6.3 **Device role per capture preset dump**
每个捕获预设的设备角色

#### 6.4 **Volume Groups dump**
音量组信息：
- 音量组ID
- 关联的音频属性
- 音量曲线
- 最小/最大音量索引

### 7. **Absolute volume devices with driving streams**
支持绝对音量控制的设备及其驱动流

### 8. **Mmap policy**
MMAP (Memory-mapped) 策略信息

### 9. **Allow playback capture log**
允许播放捕获的日志：
- Package manager 状态
- 错误计数

### 10. **Spatializer** ⭐
空间音频信息：
- 支持的级别: [NONE, MULTICHANNEL]
- 当前级别: NONE
- Head tracking 模式: [DISABLED, RELATIVE_WORLD]
- Spatialization 模式: [BINAURAL, TRANSAURAL]
- 支持的声道掩码
- Head tracking 支持状态
- 活动 Tracks 数量
- Output stream handle
- Sensor handles
- Effect handle
- 显示方向
- 立体声空间化状态
- Command log
- Pose controller 状态

### 11. **IAudioPolicyService binder call profile**
Binder 调用性能分析

---

## 关键代码位置

### Dump 函数调用链：
```
AudioPolicyService::dump()
  ↓
AudioPolicyManager::dump()
  ↓
各个子模块的 dump() 函数
```

### 主要 dump 代码文件：
1. **AudioPolicyManager.cpp** - 主 dump 函数
   - `status_t AudioPolicyManager::dump(int fd)`
   
2. **DeviceDescriptor.cpp** - 设备信息 dump
   - `void DeviceDescriptor::dump(String8 *dst, int spaces, bool verbose)`
   - `void DeviceVector::dump(String8 *dst, const String8 &tag, int spaces, bool verbose)`

3. **HwModule.cpp** - 硬件模块 dump
   - `void HwModule::dump(String8 *dst, int spaces)`

4. **IOProfile.cpp** - 输入/输出配置 dump
   - `void IOProfile::dump(String8 *dst)`

5. **AudioOutputDescriptor.cpp** - 输出流 dump
   - `void AudioOutputDescriptor::dump(String8 *dst)`

6. **AudioInputDescriptor.cpp** - 输入流 dump
   - `void AudioInputDescriptor::dump(String8 *dst)`

7. **EngineBase.cpp** - 策略引擎 dump
   - `void EngineBase::dump(String8 *dst)`

8. **VolumeGroup.cpp** - 音量组 dump
   - `void VolumeGroup::dump(String8 *dst, int spaces)`

9. **SpatializerHelper.cpp** - 空间音频 dump
   - `void SpatializerHelper::dump(String8 *dst)`

---

## 如何生成这个 Dump

### 方法1：通过 adb 命令
```bash
adb shell dumpsys media.audio_policy > audio_policy.log
```

### 方法2：通过代码调用
```cpp
AudioPolicyService::dump(fd, args);
```

### 方法3：通过 bugreport
```bash
adb bugreport
# 在 bugreport 中查找 DUMP OF SERVICE media.audio_policy
```

---

## Port ID 的生成机制

### Port ID 生成代码位置：
**文件**: `audiopolicy/common/managerdefinitions/src/PolicyAudioPort.cpp`

```cpp
audio_port_handle_t PolicyAudioPort::getNextUniqueId()
{
    return getNextHandle();
}
```

**文件**: `audiopolicy/common/managerdefinitions/src/DeviceDescriptor.cpp`

```cpp
void DeviceDescriptor::attach(const sp<HwModule>& module)
{
    PolicyAudioPort::attach(module);
    mId = getNextUniqueId();  // ⭐ 这里生成 Port ID
}
```

### Port ID 生成时机：
1. **设备连接时** (`setDeviceConnectionState`)
2. **HAL 模块加载时** (`loadHwModule`)
3. **设备 attach 到模块时** (`DeviceDescriptor::attach`)

### Port ID 特点：
- **全局唯一**：通过原子计数器生成
- **持久性**：设备连接期间保持不变
- **范围**：从 1 开始递增
- **类型**：`audio_port_handle_t` (int32_t)

### Port Hal ID vs Port ID：
- **Port ID**: AudioPolicyManager 分配的系统级ID
- **Port Hal ID**: Audio HAL 返回的硬件级ID
- 两者是不同的命名空间，互不冲突

---

## 总结

这个 dump 文件是 Android Audio 系统最重要的调试信息来源，包含了：
- ✅ 所有可用的音频设备（输入/输出）
- ✅ 当前活动的音频流
- ✅ 音频路由配置
- ✅ 音量控制信息
- ✅ 策略引擎状态
- ✅ 空间音频配置
- ✅ 硬件模块信息

通过分析这个 dump，可以诊断：
- 设备连接问题
- 音频路由问题
- 音量控制问题
- 音频策略配置问题
- HAL 初始化问题

