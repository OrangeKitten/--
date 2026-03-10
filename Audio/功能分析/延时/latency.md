这是一个非常好的关于 Android 音频延迟的问题。让我详细分析 AudioTrack 和 AudioRecord 的数据延迟情况。

[2 tools called]

让我搜索关于音频延迟的关键信息：

[2 tools called]

现在让我创建一个详细的音频延迟分析文档：

## Android 音频延迟详细分析

### 一、AudioTrack 播放延迟（App → AudioFlinger → HAL）

#### 1. **总延迟组成**

```
总延迟 = 应用层缓冲延迟 + AudioFlinger 缓冲延迟 + HAL 缓冲延迟 + 硬件延迟
```

#### 2. **典型延迟值**

| 模式 | 总延迟 | 说明 |
|------|--------|------|
| **FAST 模式** | **10-20ms** | 低延迟模式，适合游戏、实时音频 |
| **DEEP_BUFFER 模式** | **40-100ms** | 正常模式，适合音乐播放 |
| **OFFLOAD 模式** | **100-200ms** | 硬件解码，省电但延迟高 |

#### 3. **延迟计算公式**

从 AudioTrack.cpp 中可以看到：

```cpp
// frameworks/av/media/libaudioclient/AudioTrack.cpp:2086
mLatency = mAfLatency + (1000LL * mFrameCount) / mSampleRate;
```

**公式解释：**
- `mAfLatency`：AudioFlinger 报告的延迟（包含 HAL 和硬件延迟）
- `mFrameCount / mSampleRate`：AudioTrack 客户端缓冲区延迟
- 单位：毫秒（ms）

#### 4. **详细延迟分解**

```
┌─────────────────────────────────────────────────────────────┐
│                    AudioTrack 写入数据                        │
└─────────────────┬───────────────────────────────────────────┘
                  │
                  ↓ (1) 应用层缓冲延迟
┌─────────────────────────────────────────────────────────────┐
│              AudioTrack 客户端缓冲区                          │
│              延迟 = mFrameCount / mSampleRate                │
│                                                              │
│  FAST 模式:    ~256 frames / 48000 Hz = ~5.3ms             │
│  NORMAL 模式:  ~1024 frames / 48000 Hz = ~21.3ms           │
│  DEEP_BUFFER:  ~4096 frames / 48000 Hz = ~85.3ms           │
└─────────────────┬───────────────────────────────────────────┘
                  │
                  ↓ Binder IPC (~1-2ms)
┌─────────────────────────────────────────────────────────────┐
│                   AudioFlinger                               │
│                                                              │
│  (2) Mixer 缓冲延迟                                          │
│      - FAST Mixer: ~256 frames (~5.3ms @ 48kHz)            │
│      - Normal Mixer: ~1024 frames (~21.3ms @ 48kHz)        │
│                                                              │
│  (3) 处理延迟                                                │
│      - 混音处理: ~1-2ms                                      │
│      - 音效处理: ~2-5ms (如果有)                             │
└─────────────────┬───────────────────────────────────────────┘
                  │
                  ↓ HAL 接口调用 (~0.5ms)
┌─────────────────────────────────────────────────────────────┐
│                   Audio HAL                                  │
│                                                              │
│  (4) HAL 缓冲延迟                                            │
│      - HAL buffer size: 通常 256-1024 frames                │
│      - 延迟: ~5-21ms @ 48kHz                                │
└─────────────────┬───────────────────────────────────────────┘
                  │
                  ↓ 硬件传输
┌─────────────────────────────────────────────────────────────┐
│                   硬件 (DAC/Codec)                           │
│                                                              │
│  (5) 硬件延迟                                                │
│      - DAC 转换: ~1-3ms                                      │
│      - 输出缓冲: ~1-2ms                                      │
└─────────────────┬───────────────────────────────────────────┘
                  │
                  ↓
              扬声器/耳机输出
```

---

### 二、AudioRecord 录音延迟（HAL → AudioFlinger → App）

#### 1. **总延迟组成**

```
总延迟 = 硬件延迟 + HAL 缓冲延迟 + AudioFlinger 缓冲延迟 + 应用层缓冲延迟
```

#### 2. **典型延迟值**

| 模式 | 总延迟 | 说明 |
|------|--------|------|
| **FAST 模式** | **10-20ms** | 低延迟录音 |
| **NORMAL 模式** | **40-80ms** | 正常录音 |

#### 3. **延迟计算**

从 AudioRecord.cpp 中可以看到：

```cpp
// frameworks/av/media/libaudioclient/AudioRecord.cpp (set 方法中)
if (mTransfer == TRANSFER_CALLBACK || mTransfer == TRANSFER_SYNC) {
    mLatency = (1000 * mNotificationFramesAct) / mSampleRate;
} else {
    mLatency = (1000 * mFrameCount) / mSampleRate;
}
```

#### 4. **详细延迟分解**

```
              麦克风输入
                  │
                  ↓
┌─────────────────────────────────────────────────────────────┐
│                   硬件 (ADC/Codec)                           │
│                                                              │
│  (1) 硬件延迟                                                │
│      - ADC 转换: ~1-3ms                                      │
│      - 输入缓冲: ~1-2ms                                      │
└─────────────────┬───────────────────────────────────────────┘
                  │
                  ↓
┌─────────────────────────────────────────────────────────────┐
│                   Audio HAL                                  │
│                                                              │
│  (2) HAL 缓冲延迟                                            │
│      - HAL buffer size: 通常 256-1024 frames                │
│      - 延迟: ~5-21ms @ 48kHz                                │
└─────────────────┬───────────────────────────────────────────┘
                  │
                  ↓
┌─────────────────────────────────────────────────────────────┐
│                   AudioFlinger                               │
│                                                              │
│  (3) Capture 缓冲延迟                                        │
│      - FAST Capture: ~256 frames (~5.3ms @ 48kHz)          │
│      - Normal Capture: ~1024 frames (~21.3ms @ 48kHz)      │
│                                                              │
│  (4) 处理延迟                                                │
│      - 音效处理: ~2-5ms (如果有)                             │
└─────────────────┬───────────────────────────────────────────┘
                  │
                  ↓ Binder IPC (~1-2ms)
┌─────────────────────────────────────────────────────────────┐
│              AudioRecord 客户端缓冲区                         │
│              延迟 = mNotificationFramesAct / mSampleRate     │
│                                                              │
│  FAST 模式:    ~256 frames / 48000 Hz = ~5.3ms             │
│  NORMAL 模式:  ~1024 frames / 48000 Hz = ~21.3ms           │
└─────────────────┬───────────────────────────────────────────┘
                  │
                  ↓
            App 读取数据
```

---

### 三、影响延迟的关键因素

#### 1. **缓冲区大小（最重要）**

**AudioTrack 缓冲区：**
```cpp
// FAST 模式
frameCount = 256 frames
延迟 = 256 / 48000 = 5.3ms

// NORMAL 模式  
frameCount = 1024 frames
延迟 = 1024 / 48000 = 21.3ms

// DEEP_BUFFER 模式
frameCount = 4096 frames
延迟 = 4096 / 48000 = 85.3ms
```

**影响：**
- 缓冲区越大，延迟越高，但越不容易出现 underrun（播放）或 overrun（录音）
- 缓冲区越小，延迟越低，但需要更频繁的数据传输，CPU 负载更高

#### 2. **音频标志（Flags）**

| Flag | 延迟影响 | 说明 |
|------|---------|------|
| `AUDIO_OUTPUT_FLAG_FAST` | **最低延迟** | 使用 FastMixer，256 frames buffer |
| `AUDIO_OUTPUT_FLAG_DEEP_BUFFER` | **高延迟** | 省电模式，4096 frames buffer |
| `AUDIO_OUTPUT_FLAG_PRIMARY` | **中等延迟** | 默认模式，1024 frames buffer |
| `AUDIO_INPUT_FLAG_FAST` | **最低延迟** | 快速录音，256 frames buffer |

#### 3. **采样率**

```
延迟 = frameCount / sampleRate

48000 Hz: 1024 frames = 21.3ms
44100 Hz: 1024 frames = 23.2ms
16000 Hz: 1024 frames = 64ms
```

**影响：** 采样率越高，相同帧数的延迟越低

#### 4. **音频处理链**

```
无音效:     基础延迟
音效处理:   +2-5ms
重采样:     +1-3ms
格式转换:   +1-2ms
混音:       +1-2ms
```

#### 5. **系统负载**

- **CPU 负载高**：可能导致 AudioFlinger 线程调度延迟，增加 5-20ms
- **内存压力**：可能导致页面交换，增加 10-50ms
- **中断延迟**：高优先级中断可能延迟音频处理

#### 6. **硬件特性**

| 硬件 | 延迟 |
|------|------|
| **内置 Codec** | 2-5ms |
| **USB 音频** | 5-15ms |
| **蓝牙 A2DP** | 100-200ms |
| **蓝牙 LE Audio** | 20-40ms |

---

### 四、实际测量示例

#### 示例 1：音乐播放（DEEP_BUFFER）

```
应用层缓冲:    85ms  (4096 frames @ 48kHz)
AudioFlinger:  21ms  (1024 frames mixer buffer)
HAL 缓冲:      21ms  (1024 frames)
硬件延迟:      3ms
─────────────────────
总延迟:        130ms
```

#### 示例 2：游戏音效（FAST）

```
应用层缓冲:    5.3ms  (256 frames @ 48kHz)
AudioFlinger:  5.3ms  (256 frames fast mixer)
HAL 缓冲:      5.3ms  (256 frames)
硬件延迟:      2ms
─────────────────────
总延迟:        18ms
```

#### 示例 3：语音通话（FAST + 低延迟优化）

```
录音延迟:
  硬件:        2ms
  HAL:         5.3ms (256 frames)
  AudioFlinger: 5.3ms
  应用层:      5.3ms
  小计:        18ms

播放延迟:
  应用层:      5.3ms
  AudioFlinger: 5.3ms
  HAL:         5.3ms
  硬件:        2ms
  小计:        18ms

往返延迟(RTT): 36ms
```

---

### 五、优化建议

#### 1. **低延迟场景（游戏、实时音频）**

```java
AudioTrack track = new AudioTrack.Builder()
    .setAudioAttributes(new AudioAttributes.Builder()
        .setUsage(AudioAttributes.USAGE_GAME)
        .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
        .setFlags(AudioAttributes.FLAG_LOW_LATENCY)  // 关键
        .build())
    .setAudioFormat(new AudioFormat.Builder()
        .setSampleRate(48000)
        .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
        .setChannelMask(AudioFormat.CHANNEL_OUT_STEREO)
        .build())
    .setBufferSizeInBytes(256 * 2 * 2)  // 256 frames, 小缓冲
    .setTransferMode(AudioTrack.MODE_STREAM)
    .build();
```

**预期延迟：** 10-20ms

#### 2. **音乐播放场景**

```java
AudioTrack track = new AudioTrack.Builder()
    .setAudioAttributes(new AudioAttributes.Builder()
        .setUsage(AudioAttributes.USAGE_MEDIA)
        .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
        .build())
    .setAudioFormat(new AudioFormat.Builder()
        .setSampleRate(48000)
        .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
        .setChannelMask(AudioFormat.CHANNEL_OUT_STEREO)
        .build())
    .setBufferSizeInBytes(4096 * 2 * 2)  // 4096 frames, 大缓冲
    .setTransferMode(AudioTrack.MODE_STREAM)
    .build();
```

**预期延迟：** 80-120ms（但更省电，不易 underrun）

#### 3. **录音优化**

```java
AudioRecord record = new AudioRecord.Builder()
    .setAudioSource(MediaRecorder.AudioSource.VOICE_COMMUNICATION)
    .setAudioFormat(new AudioFormat.Builder()
        .setSampleRate(48000)
        .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
        .setChannelMask(AudioFormat.CHANNEL_IN_MONO)
        .build())
    .setBufferSizeInBytes(256 * 1 * 2)  // 小缓冲，低延迟
    .build();
```

---

### 六、延迟监控

#### 获取延迟信息

```java
// AudioTrack
int latency = audioTrack.getLatency();  // 毫秒

// AudioRecord  
// 没有直接 API，需要通过 timestamp 计算
AudioTimestamp timestamp = new AudioTimestamp();
audioRecord.getTimestamp(timestamp, AudioTimestamp.TIMEBASE_MONOTONIC);
```

---

### 七、总结

| 场景 | 推荐模式 | 预期延迟 | 缓冲区大小 |
|------|---------|---------|-----------|
| **游戏音效** | FAST | 10-20ms | 256 frames |
| **VoIP 通话** | FAST | 15-30ms | 256-512 frames |
| **音乐播放** | DEEP_BUFFER | 80-120ms | 4096 frames |
| **视频播放** | PRIMARY | 40-80ms | 1024-2048 frames |
| **录音** | FAST | 10-20ms | 256 frames |

**关键要点：**
1. 延迟主要由**缓冲区大小**决定
2. FAST 模式可以达到 **10-20ms** 的低延迟
3. 低延迟需要更高的 CPU 负载
4. 需要在延迟、功耗、稳定性之间权衡