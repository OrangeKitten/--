# AudioFlinger Dump 文件结构总结

## 文件来源
这个 dump 是由 **AudioFlinger** 生成的，打印代码位于：
- **主文件**: `audioflinger/AudioFlinger.cpp`
- **dump() 函数**: 当执行 `dumpsys media.audio_flinger` 命令时调用

---

## Dump 文件包含的主要信息模块

### 1. **Libraries NOT loaded**
未加载的音效库列表

### 2. **Libraries loaded** ⭐
已加载的音效库及其效果器，包括：

#### 2.1 Qualcomm 音效库
- **audio_synth** - 合成器
- **audio_sumx** - SUMX 效果
- **audio_avc** - AVC (自动音量控制)
- **audio_volume** - 音量控制
- **audio_delay** - 延迟效果
- **audio_fnb** - Fade-n-Balance (淡入淡出和平衡)
- **audio_bmt** - Bass-Mid-Treble (低中高音调节)
- **shoebox** - Shoebox 效果
- **audiosphere** - Audiosphere 效果
- **audio_pre_processing** - 预处理
  - Noise Suppression (降噪)
  - Acoustic Echo Canceler (回声消除)
- **offload_bundle** - Offload 效果包

#### 2.2 NXP 音效库
- **proxy** - 代理效果器
  - Visualizer (可视化器)
  - Insert/Auxiliary Preset Reverb (预设混响)
  - Insert/Auxiliary Environmental Reverb (环境混响)
  - Equalizer (均衡器)
  - Virtualizer (虚拟环绕声)
  - Dynamic Bass Boost (动态低音增强)

#### 2.3 AOSP 音效库
- **dynamics_processing** - 动态处理
- **loudness_enhancer** - 响度增强
- **downmix** - 降混
- **visualizer_hw/sw** - 硬件/软件可视化器
- **reverb** - 混响
- **bundle** - 效果包
  - Volume (音量)

每个效果器包含：
- UUID (唯一标识符)
- TYPE (类型标识)
- apiVersion (API版本)
- flags (标志位)

### 3. **XML effect configuration**
XML 效果配置加载状态
- 部分加载，跳过了 4 个元素

### 4. **Clients**
客户端信息：
- pid: 5401

### 5. **Notification Clients** ⭐
通知客户端列表，包括：
- pid (进程ID)
- uid (用户ID)
- name (进程名称)

**示例**：
- 922 (uid: 1000) - android.uid.system
- 1105 (uid: 1013) - media
- 6307 (uid: 1041) - audioserver

### 6. **Global session refs**
全局会话引用：
- session (会话ID)
- cnt (引用计数)
- pid (进程ID)
- uid (用户ID)
- name (进程名称)

**示例**：
- session 161, 153, 169, 177

### 7. **Hardware status**
硬件状态：0

### 8. **Standby Time mSec**
待机时间：1000 毫秒

---

## 9. **Output threads** ⭐⭐⭐ (核心部分)

系统中所有输出线程的详细信息，每个线程包含：

### 线程基本信息
- **Thread address** (线程地址)
- **Thread name** (线程名称，如 AudioOut_D, AudioOut_15)
- **tid** (线程ID)
- **type** (线程类型)
  - 0 = MIXER (混音器)
  - 5 = MMAP_PLAYBACK (内存映射播放)

### 音频配置
- **I/O handle** (I/O句柄)
- **Standby** (待机状态: yes/no)
- **Sample rate** (采样率: 8000-48000 Hz)
- **HAL frame count** (HAL帧数)
- **HAL format** (HAL格式)
  - 0x1 = AUDIO_FORMAT_PCM_16_BIT
  - 0x3 = AUDIO_FORMAT_PCM_32_BIT
- **HAL buffer size** (HAL缓冲区大小)
- **Channel count** (声道数: 2, 10, 12)
- **Channel mask** (声道掩码)
- **Processing format** (处理格式)
- **Processing frame size** (处理帧大小)

### 设备信息
- **Output devices** (输出设备)
  - 0x1000000 = AUDIO_DEVICE_OUT_BUS
- **Input device** (输入设备)
- **Audio source** (音频源)

### 性能统计
- **Timestamp stats** (时间戳统计)
- **Timestamp corrected** (时间戳校正)
- **Master volume** (主音量)
- **Master mute** (主静音)
- **Mixer channel Mask** (混音器声道掩码)
- **Normal frame count** (正常帧数)
- **Total writes** (总写入次数)
- **Delayed writes** (延迟写入次数)
- **Blocked in write** (写入阻塞状态)
- **Suspend count** (挂起计数)

### 缓冲区信息
- **Sink buffer** (接收缓冲区地址)
- **Mixer buffer** (混音缓冲区地址)
- **Effect buffer** (效果缓冲区地址)
- **Fast track availMask** (快速轨道可用掩码)
- **Standby delay ns** (待机延迟纳秒)

### 流信息
- **AudioStreamOut** (音频输出流地址)
- **flags** (标志)
  - 0x0 = AUDIO_OUTPUT_FLAG_NONE
  - 0x2 = AUDIO_OUTPUT_FLAG_PRIMARY
- **Frames written** (已写入帧数)
- **Suspended frames** (挂起帧数)

### HAL 流信息
- **Hal stream dump** (HAL流转储)
- **Signal power history** (信号功率历史)

### 混音器信息
- **Thread throttle time** (线程节流时间)
- **AudioMixer tracks** (音频混音器轨道)
- **Master mono** (主单声道)
- **Master balance** (主平衡)
- **FastMixer** (快速混音器状态)

### 音量信息
- **Stream volumes in dB** (各流类型的音量，单位dB)
  - 0-14 对应不同的流类型
  - -inf 表示静音
  - -6, 0 等表示具体音量值

### 性能计数器
- **Normal mixer raw underrun counters** (混音器欠载计数器)
  - partial (部分欠载)
  - empty (空欠载)

### 活动轨道
- **Tracks** (轨道数量及详细信息)
  - Type (类型)
  - Id (ID)
  - Active (活动状态)
  - Client (客户端PID)
  - Session (会话ID)
  - Port Id (端口ID)
  - Flags (标志)
  - Format (格式)
  - Channel mask (声道掩码)
  - Sample Rate (采样率)
  - Stream Type (流类型)
  - Usage (使用场景)
  - Content Type (内容类型)
  - Gain (增益)
  - Volume (音量: L/R/VS)
  - Server Frame Count (服务器帧数)
  - Frame Ready (就绪帧数)
  - Underruns (欠载次数)
  - Flushed (刷新次数)
  - Latency (延迟)

### 效果链
- **Effect Chains** (效果链数量)

### 本地日志
- **Local log** (本地日志)
  - 时间戳
  - 事件类型 (如 CFG_EVENT_CREATE_AUDIO_PATCH)
  - 设备变化信息

---

## 本例中的输出线程统计

### 共有 **18 个活动输出线程**：

| 线程名称 | I/O Handle | 采样率 | 格式 | 声道数 | 类型 |
|---------|-----------|--------|------|--------|------|
| AudioOut_D | 13 | 48000 Hz | PCM_16_BIT | 2 | MIXER |
| AudioOut_15 | 21 | 48000 Hz | PCM_16_BIT | 2 | MIXER |
| AudioOut_1D | 29 | 48000 Hz | PCM_16_BIT | 2 | MIXER (PRIMARY) |
| AudioOut_25 | 37 | 48000 Hz | PCM_32_BIT | 10 | MIXER (PRIMARY) |
| AudioOut_2D | 45 | 48000 Hz | PCM_32_BIT | 12 | MIXER (PRIMARY) |
| AudioOut_35 | 53 | 48000 Hz | PCM_16_BIT | 2 | MIXER |
| AudioOut_3D | 61 | 48000 Hz | PCM_16_BIT | 2 | MIXER |
| AudioOut_45 | 69 | 48000 Hz | PCM_16_BIT | 2 | MIXER |
| AudioOut_4D | 77 | 48000 Hz | PCM_16_BIT | 2 | MIXER |
| AudioOut_55 | 85 | 48000 Hz | PCM_16_BIT | 2 | MIXER |
| AudioOut_5D | 93 | 8000 Hz | PCM_16_BIT | 2 | MIXER |
| AudioOut_65 | 101 | 16000 Hz | PCM_16_BIT | 2 | MIXER |
| AudioOut_6D | 109 | 24000 Hz | PCM_16_BIT | 2 | MIXER |
| AudioOut_75 | 117 | 32000 Hz | PCM_16_BIT | 2 | MIXER |
| AudioOut_7D | 125 | 48000 Hz | PCM_16_BIT | 2 | MIXER |
| AudioOut_85 | 133 | 48000 Hz | PCM_16_BIT | 2 | MIXER |
| AudioOut_8D | 141 | 48000 Hz | PCM_16_BIT | 2 | MIXER |
| AudioOut_95 | 149 | 48000 Hz | PCM_16_BIT | 2 | MIXER |
| AudioOut_9D | 157 | 48000 Hz | PCM_16_BIT | 2 | MIXER |
| AudioOut_A5 | 165 | 48000 Hz | PCM_16_BIT | 2 | MIXER |
| AudioOut_B5 | 181 | 44100 Hz | PCM_16_BIT | 2 | MIXER |

**特点**：
- 所有线程都处于 **Standby** 状态
- 大部分采样率为 **48000 Hz**
- 支持多种采样率：8000, 16000, 24000, 32000, 44100, 48000 Hz
- 支持 **10声道** 和 **12声道** 的多声道输出
- 所有设备都是 **AUDIO_DEVICE_OUT_BUS** 类型

---

## 10. **USB audio module**
USB 音频模块状态：
- No output streams (无输出流)
- No input streams (无输入流)

---

## 11. **Reroute submix audio module**
重路由 submix 音频模块：
- 10 个路由配置 (route[0] - route[9])
- route[9] 活动：rate in=48000 out=48000

---

## 12. **Patches** ⭐
音频补丁（路由连接）信息：

共 **21 个 Patches**，每个包含：
- Patch handle (补丁句柄)
- Software bridge 状态
- Thread 地址
- Sink 数量
- Device type (设备类型: 01000000 = AUDIO_DEVICE_OUT_BUS)

**示例**：
- Patch 68, 92, 100, 108, 116, 124, 148, 156, 164, 180, 188, 196, 204, 212, 220, 228, 236, 244, 252, 260, 268

---

## 13. **Device Effects**
设备效果（空）

---

## 14. **Rejected setParameters**
被拒绝的参数设置（空）

---

## 15. **App setParameters**
应用程序参数设置（空）

---

## 16. **System setParameters** ⭐
系统参数设置历史：
- 时间戳
- UID
- Key-Value Pairs (KVP)

**示例**：
```
01-01 08:00:33.403 UID 1000: restarting=true
01-01 08:00:33.407 UID 1000: cameraFacing=front
01-01 08:00:33.676 UID 1000: BT_SCO=off
01-01 08:00:34.809 UID 1000: restarting=false
```

---

## 17. **Historical Thread Log** ⭐
历史线程日志（已删除的线程）：

### 17.1 MMAP 输出线程
- **AudioMmapOut_AD** (I/O handle: 173)
  - 8000 Hz, PCM_16_BIT, 2声道
  - MMAP_PLAYBACK 类型

### 17.2 输入线程（AudioRecord）
记录了 4 个已删除的输入线程：

#### AudioIn_16 (I/O handle: 22)
- 48000 Hz, PCM_16_BIT
- **14 声道** (0x80003fff)
- Buffer: 26880 bytes
- 用途：多麦克风阵列录音

#### AudioIn_1E (I/O handle: 30)
- 48000 Hz, PCM_16_BIT
- **3 声道** (0x80000007)
- Buffer: 5760 bytes
- 用途：三麦克风录音

#### AudioIn_2E (I/O handle: 46)
- 48000 Hz, PCM_16_BIT
- **1 声道** (单声道)
- Buffer: 1920 bytes
- 用途：单麦克风录音

#### AudioIn_4E (I/O handle: 78)
- 48000 Hz, PCM_16_BIT
- **2 声道** (立体声)
- Buffer: 4096 bytes
- 用途：立体声录音

**特点**：
- 所有输入线程都是 **RECORD** 类型
- Standby: no (已创建)
- Frames read: 0 (未读取数据)
- No active record clients (无活动录音客户端)

---

## 关键代码位置

### Dump 函数调用链：
```
AudioFlinger::dump()
  ↓
dumpEffects()
  ↓
dumpClients()
  ↓
PlaybackThread::dump()
  ↓
RecordThread::dump()
  ↓
AudioStreamOut::dump()
  ↓
AudioStreamIn::dump()
```

### 主要 dump 代码文件：
1. **AudioFlinger.cpp** - 主 dump 函数
   - `status_t AudioFlinger::dump(int fd, const Vector<String16>& args)`
   
2. **Threads.cpp** - 线程信息 dump
   - `void AudioFlinger::PlaybackThread::dump(int fd, const Vector<String16>& args)`
   - `void AudioFlinger::RecordThread::dump(int fd, const Vector<String16>& args)`

3. **Tracks.cpp** - 轨道信息 dump
   - `void AudioFlinger::PlaybackThread::Track::dump(char* buffer, size_t size, bool active)`

4. **Effects.cpp** - 效果器信息 dump
   - `void AudioFlinger::EffectModule::dump(int fd, const Vector<String16>& args)`

5. **AudioStreamOut.cpp / AudioStreamIn.cpp** - HAL 流 dump
   - HAL 层的流信息转储

---

## 如何生成这个 Dump

### 方法1：通过 adb 命令
```bash
adb shell dumpsys media.audio_flinger > audio_flinger.log
```

### 方法2：通过代码调用
```cpp
AudioFlinger::dump(fd, args);
```

### 方法3：通过 bugreport
```bash
adb bugreport
# 在 bugreport 中查找 DUMP OF SERVICE media.audio_flinger
```

---

## 与 AudioPolicyManager Dump 的区别

| 特性 | AudioFlinger | AudioPolicyManager |
|------|--------------|-------------------|
| **层级** | HAL 层 / 服务层 | 策略层 |
| **关注点** | 实际的音频流、线程、混音 | 设备管理、路由策略 |
| **输出线程** | 详细的线程状态、缓冲区 | 输出流的策略配置 |
| **设备信息** | 实际连接的设备 | 可用设备列表 |
| **音效** | 加载的音效库和实例 | 音效策略 |
| **性能** | 欠载、延迟、缓冲区 | 音量、路由决策 |

---

## 调试用途

通过分析这个 dump，可以诊断：
- ✅ 音频线程状态和性能
- ✅ 音频流的配置和状态
- ✅ 音效库加载情况
- ✅ 缓冲区欠载问题
- ✅ 音频路由连接
- ✅ 客户端连接状态
- ✅ 历史线程和流信息
- ✅ 系统参数变化历史

---

## 总结

AudioFlinger dump 是 Android 音频系统底层运行状态的完整快照，包含：
- 🎵 18 个活动输出线程
- 🎤 4 个历史输入线程（已删除）
- 🎛️ 20+ 个音效库
- 🔗 21 个音频补丁
- 📊 详细的性能统计
- 📝 系统参数变化历史

这是诊断音频播放、录音、音效、性能问题的最重要工具！

