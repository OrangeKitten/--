# PlaybackThread::threadLoop 写入控制机制分析

  

> 源码路径：`frameworks/av/services/audioflinger/Threads.cpp`

> 分析版本：QSSI.16

  

---

  

## 一、整体架构概述

  

`PlaybackThread::threadLoop` 是 AudioFlinger 中负责音频播放的核心循环函数。它从 App 端的 AudioTrack（通过 SharedBuffer / CircularRecord）获取 PCM 数据，经过混音（Mixing）和音效处理（Effects）后，最终写入 HAL（音频硬件抽象层）。

  

### 数据流向

  

```

App 写入数据到 SharedBuffer (AudioTrack)

        │

        ▼

prepareTracks_l()          ← 检查 track 数据是否就绪

        │

        ▼

threadLoop_mix()            ← 执行混音，或

threadLoop_sleepTime()      ← 决定是否休眠

        │

        ▼

Effects 处理链               ← 可选的音效处理

        │

        ▼

threadLoop_write()          ← 写入 HAL (MonoPipe / AudioStreamOut)

        │

        ▼

HAL (Audio Hardware)

```

  

---

  

## 二、核心变量说明

  

| 变量名 | 类型 | 说明 |

|--------|------|------|

| `mBytesRemaining` | size_t | 剩余待写入字节数，为 0 时触发新一轮混合 |

| `mCurrentWriteLength` | size_t | 本次循环要写入的总字节数（通常为 mSinkBufferSize） |

| `mBytesWritten` | int64_t | 历史累计写入字节数（用于诊断） |

| `mSleepTimeUs` | uint32_t | 本次循环睡眠时间（微秒），为 0 表示必须写入 |

| `mMixerStatus` | mixer_state | 混音状态：MIXER_IDLE / MIXER_TRACKS_READY / MIXER_TRACKS_ENABLED / MIXER_DRAIN_TRACK / MIXER_DRAIN_ALL |

| `mStandbyTimeNs` | nsecs_t | 进入 standby 的时间戳 |

| `framesReady` | size_t | SharedBuffer 中可用数据帧数 = rear - front |

  

---

  

## 三、主循环结构（threadLoop 骨架）

  

```cpp

// Threads.cpp:4191

bool PlaybackThread::threadLoop()

{

    for (int64_t loopCount = 0; !exitPending(); ++loopCount) {

  

        // 1. MSD 延迟检测、设备检查

        // ...

  

        // 2. 处理配置事件

        processConfigEvents_l();

  

        // 3. 收集时间戳

        collectTimestamps_l();

  

        // 4. 等待异步回调完成（如果有）

        if (waitingAsyncCallback_l()) {

            mWaitWorkCV.wait_for(...);  // 等待异步写完成

            continue;

        }

  

        // 5. 无活跃 Track → 进入 standby / 睡眠等待

        if (mActiveTracks.isEmpty() && systemTime() > mStandbyTimeNs) {

            threadLoop_standby();

            mWaitWorkCV.wait(_l);  // 阻塞等待被唤醒

            continue;

        }

  

        // 6. 核心：准备 Track，决定混音状态

        mMixerStatus = prepareTracks_l(&tracksToRemove);

  

        // 7. 混音或决定睡眠时间

        if (mBytesRemaining == 0) {

            if (mMixerStatus == MIXER_TRACKS_READY) {

                threadLoop_mix();       // 混音，设置 mSleepTimeUs = 0

            } else {

                threadLoop_sleepTime(); // 无数据则设置睡眠时间

            }

        }

  

        // 8. Effects 处理

        for (effectChain : effectChains) {

            effectChain->process_l();

        }

  

        // 9. 写入 HAL

        if (mSleepTimeUs == 0 && mBytesRemaining > 0) {

            ret = threadLoop_write();    // 实际写入

        } else if (mSleepTimeUs > 0) {

            usleep(mSleepTimeUs);        // 休眠

        }

  

        // 10. ThreadThrottle 限速

        // ...

  

        // 11. 移除已删除的 Track

        threadLoop_removeTracks(tracksToRemove);

    }

}

```

  

---

  

## 四、数据就绪判断（prepareTracks_l）

  

源码位置：`Threads.cpp:6201`

  

```cpp

size_t framesReady = track->framesReady();  // 从 SharedBuffer 获取可用帧数

  

// framesReady 计算方式（AudioTrackServerProxy::framesReady）：

//     filled = rear(写指针) - front(读指针)

// 即：已写入但尚未被 AudioFlinger 消费的数据量

  

if ((framesReady >= minFrames) && track->isReady() && ...) {

    // 数据就绪，Track 可以被混合

    mixerStatus = MIXER_TRACKS_READY;

} else {

    // underrun 统计

    if (framesReady < desiredFrames && !track->isStopped() && !track->isPaused()) {

        ALOGW("track(%d) underrun, framesReady(%zu) < framesDesired(%zd)", ...);

    }

}

```

  

### minFrames 的含义

  

```cpp

// Threads.cpp:6195-6199

uint32_t minFrames = 1;

if ((track->sharedBuffer() == 0) && !track->isStopped() &&

    (mMixerStatusIgnoringFastTracks == MIXER_TRACKS_READY)) {

    // 只有在上一轮也是 READY 时，才要求完整缓冲区

    // 否则只要求 1 帧即可启动

    minFrames = desiredFrames;

}

```

  

---

  

## 五、写入触发条件（mSleepTimeUs 的作用）

  

### 条件分支

  

| mSleepTimeUs | mBytesRemaining | 行为 |

|-------------|---------------|------|

| == 0 | > 0 | 执行 threadLoop_write()，写入 HAL |

| > 0 | 任意 | usleep(mSleepTimeUs)，不写入 |

| == 0 | == 0 | 执行 threadLoop_mix() 重新混音 |

  

### threadLoop_mix()

  

```cpp

// Threads.cpp:5738

void MixerThread::threadLoop_mix()

{

    mAudioMixer->process();                    // 执行混音

    mCurrentWriteLength = mSinkBufferSize;     // 设置写入大小

    mSleepTimeUs = 0;                          // 标记必须写入

    mStandbyTimeNs = systemTime() + mStandbyDelayNs;

}

```

  

### threadLoop_sleepTime()

  

```cpp

// Threads.cpp:5756

void MixerThread::threadLoop_sleepTime()

{

    if (mSleepTimeUs == 0) {

        if (mMixerStatus == MIXER_TRACKS_ENABLED) {

            // 有 Track 但数据不足，计算等待时间

            if (mPipeSink != nullptr && mPipeSink == mNormalSink) {

                // 使用 MonoPipe 估算等待时间

                ssize_t availableToWrite = mPipeSink->availableToWrite();

                size_t framesDelay = std::min(

                    mNormalFrameCount,

                    max(framesLeft / 2, mFrameCount));

                mSleepTimeUs = framesDelay * MICROS_PER_SECOND / mSampleRate;

            } else {

                mSleepTimeUs = mActiveSleepTimeUs >> sleepTimeShift;

            }

        } else {

            mSleepTimeUs = mIdleSleepTimeUs;

        }

    }

}

```

  

---

  

## 六、写入层级的阻塞控制（重点）

  

### 6.1 路径一：MonoPipe（NomalMixer with FastMixer）

  

当使用 FastMixer 架构时，数据通过 MonoPipe 传输。

  

**MonoPipe 创建**（`Threads.cpp:5430`）：

```cpp

MonoPipe *monoPipe = new MonoPipe(

    mNormalFrameCount * 4,   // 容量 = 4 倍正常缓冲

    format,

    true                      // writeCanBlock = 阻塞写入

);

```

  

**MonoPipe 写入逻辑**（`MonoPipe.cpp:66`）：

  

```cpp

ssize_t MonoPipe::write(const void *buffer, size_t count)

{

    while (count > 0) {

        ssize_t actual = mFifoWriter.write(buffer, count);

  

        if (!mWriteCanBlock || mIsShutdown) {

            break;  // 非阻塞或已关闭，直接返回

        }

  

        // 根据 Pipe 填充深度自适应调整睡眠时间（限流）

        size_t filled = mMaxFrames - avail;  // 已填充帧数

  

        if (filled <= mSetpoint / 2) {

            // Pipe 接近空，快速填充

            ns = written * ( 500000000 / sampleRate);  // 0.5x 速率

        } else if (filled <= (mSetpoint * 3) / 4) {

            ns = written * ( 750000000 / sampleRate);  // 0.75x

        } else if (filled <= (mSetpoint * 5) / 4) {

            ns = written * (1000000000 / sampleRate);  // 1.0x 正常速率

        } else if (filled <= (mSetpoint * 3) / 2) {

            ns = written * (1150000000 / sampleRate);  // 1.15x 稍慢

        } else if (filled <= (mSetpoint * 7) / 4) {

            ns = written * (1350000000 / sampleRate);  // 1.35x 慢

        } else {

            // Pipe 严重溢出，最慢速率

            ns = written * (1750000000 / sampleRate);  // 1.75x 最慢

        }

  

        nanosleep(ns);  // 限流睡眠

    }

}

```

  

**关键设计**：MonoPipe 容量为 `mNormalFrameCount * 4`（4倍缓冲）。当 Pipe 接近满时，写入速度通过 `nanosleep` 限流（最多 1.75x 正常速率），**不会直接返回失败**，而是智能降速。

  

### 6.2 路径二：HAL 直接写入（非 NBAIO sink）

  

```cpp

// Threads.cpp:3784

bytesWritten = mOutput->write((char *)mSinkBuffer + offset, mBytesRemaining);

// HAL 的 write() 内部会阻塞等待硬件就绪

```

  

---

  

## 七、用户写入快于消费时的完整控制流程

  

当 App 写入速度 > HAL 消费速度时，数据不会丢失，而是通过多层机制协同限流：

  

```

App 快速 write() → SharedBuffer 填充增加

        │

        ▼

framesReady = rear - front 增加（mFront 不动）

        │

        ▼

prepareTracks_l: framesReady >= minFrames → MIXER_TRACKS_READY

        │

        ▼

threadLoop_mix → mSinkBuffer 被填充

        │

        ▼

threadLoop_write → 写入 MonoPipe

        │

        ├──→ MonoPipe 填充深度增加

        │         │

        │         ▼

        │    写入睡眠时间增加（0.5x → 1.75x）

        │         │

        │         ▼

        │    写入速度被限流，不会淹没下游

        │

        └──→ 同时 App 端 SharedBuffer 变满

                  │

                  ▼

             App write() 阻塞在 cblk()->cv.wait()

             （SharedBuffer 满，等待 mFront 前进）

```

  

### 关键：App 端反压机制

  

当 SharedBuffer 写满后，App 侧的 AudioTrack::write() 会阻塞在条件变量上：

  

```cpp

// App 侧（客户端）

while (framesReady >= bufferSize) {

    cblk()->cv.wait();  // 等待 AF 消费数据后唤醒

}

```

  

---

  

## 八、写入失败处理

  

```cpp

// Threads.cpp:4708-4767

ret = threadLoop_write();

  

if (ret < 0) {

    mBytesRemaining = 0;  // 丢弃剩余数据，下次循环重试

} else if (ret > 0) {

    mBytesWritten += ret;

    mBytesRemaining -= ret;

    mFramesWritten += ret / mFrameSize;

}

  

// 写入阻塞检测

int64_t deltaWriteNs = lastIoEndNs - lastIoBeginNs;

if (deltaWriteNs > maxPeriod) {  // maxPeriod = frameCount / sampleRate * 15

    mNumDelayedWrites++;

    ALOGW("write blocked for %lld msecs, %d delayed writes",

          deltaWriteNs / NANOS_PER_MILLISECOND, mNumDelayedWrites);

}

```

  

---

  

## 九、ThreadThrottle 限速机制

  

防止 MixerThread 处理速率过快（超过 HAL 消费速率 2 倍）：

  

```cpp

// Threads.cpp:4773-4829

if (mType == MIXER && !mStandby && mThreadThrottle) {

    const int32_t deltaMs = writePeriodNs / NANOS_PER_MILLISECOND;

    const int32_t throttleMs = (int32_t)mHalfBufferMs - deltaMs;

  

    if (mHalfBufferMs >= throttleMs && throttleMs > 0) {

        // 处理过快，强制休眠以匹配 HAL 消费速率

        usleep(throttleMs * 1000);

    }

}

```

  

`mHalfBufferMs = mNormalFrameCount * 1000 / (2 * mSampleRate)` — 半缓冲时长。

  

---

  

## 十、关键设计原则总结

  

| 机制 | 实现方式 | 作用 |

|------|---------|------|

| **MonoPipe 限流** | nanosleep + 自适应填充深度检测 | Pipe 满时智能降速，不丢数据 |

| **SharedBuffer 反压** | App 侧条件变量等待 | 从源头阻止 App 过快写入 |

| **mSleepTimeUs 休眠** | 无数据时主动睡眠 | 避免空转，节省 CPU |

| **ThreadThrottle** | usleep 强制匹配 HAL 速率 | 防止处理过快导致 underrun |

| **mBytesRemaining 分批** | 单次写入失败不影响后续 | 提供容错能力 |

  

**核心设计哲学**：**写入不会"失败丢失数据"，而是通过阻塞等待来限流**，保护下游消费者（HAL）不被淹没。整个链路通过多级反压机制实现自适应的速度匹配。

  

---

  

## 十一、相关源码文件索引

  

| 文件 | 关键内容 |

|------|---------|

| `Threads.cpp:4191` | PlaybackThread::threadLoop 主循环入口 |

| `Threads.cpp:5815` | MixerThread::prepareTracks_l Track 就绪判断 |

| `Threads.cpp:5738` | MixerThread::threadLoop_mix 混音执行 |

| `Threads.cpp:5756` | MixerThread::threadLoop_sleepTime 睡眠时间计算 |

| `Threads.cpp:3729` | PlaybackThread::threadLoop_write 写入 HAL |

| `MonoPipe.cpp:66` | MonoPipe::write 阻塞写入 + 限流逻辑 |

| `fifo.cpp:180` | audio_utils_fifo_writer::write FIFO 实际写入 |

| `AudioTrackShared.cpp:969` | AudioTrackServerProxy::framesReady 数据就绪判断 |