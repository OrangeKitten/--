# AudioTrack Static 模式数据流转完整分析

> **分析目标**: 深入理解 AudioTrack static 模式如何将数据一次性写入 Track，并通过 threadLoop_write 写入 AudioHAL，以及播放完成后如何停止写入

**文档版本**: v1.0
**创建日期**: 2025-02-10
**分析基于**: Android Audio Framework 源码

---

## 📋 目录

1. [概述](#概述)
2. [Static Buffer 的创建和传递](#static-buffer-的创建和传递)
3. [数据一次性写入机制](#数据一次性写入机制)
4. [PlaybackThread::threadLoop 处理逻辑](#playbackthreadthreadloop-处理逻辑)
5. [数据从 Track 到 HAL 的流转路径](#数据从-track-到-hal-的流转路径)
6. [播放完成控制机制](#播放完成控制机制)
7. [循环播放控制](#循环播放控制)
8. [关键代码位置索引](#关键代码位置索引)
9. [总结](#总结)

---

## 概述

### 什么是 Static 模式？

AudioTrack 的 **Static 模式**（也称为 Shared Buffer 模式）是一种特殊的音频播放模式，适用于：
- **短音效播放**（如按键音、提示音、通知音）
- **需要循环播放的音频**
- **预先加载完整音频数据的场景**

### Static 模式 vs Streaming 模式对比

| 特性 | Streaming 模式 | Static 模式 |
|------|---------------|-------------|
| **内存分配** | AudioFlinger 分配共享内存 | 应用层提供 IMemory |
| **数据提供** | 应用不断调用 `write()` 填充数据 | 一次性加载完整音频数据 |
| **Transfer 类型** | `TRANSFER_SYNC`/`TRANSFER_CALLBACK` | `TRANSFER_SHARED` |
| **客户端 Proxy** | `AudioTrackClientProxy` | `StaticAudioTrackClientProxy` |
| **服务端 Proxy** | `AudioTrackServerProxy` | `StaticAudioTrackServerProxy` |
| **数据拷贝** | 需要拷贝到混音缓冲区 | 直接从 shared buffer 读取（零拷贝）|
| **循环播放** | ❌ 不支持 | ✅ 支持（loopCount, loopStart, loopEnd）|
| **下溢处理** | 重试机制，计数下溢帧 | 到达结束即完成，无下溢 |
| **minFrames** | = desiredFrames（需足够数据）| = 1（有数据即可）|

---

## Static Buffer 的创建和传递

### 1. 客户端构造 AudioTrack

**文件**: `d:\code\Audio相关\framework\libaudioclient\AudioTrack.cpp`

```cpp
// 行: 308-324
AudioTrack(
    audio_stream_type_t streamType,
    uint32_t sampleRate,
    audio_format_t format,
    audio_channel_mask_t channelMask,
    const sp<IMemory>& sharedBuffer,  // ← Static buffer 参数
    audio_output_flags_t flags = AUDIO_OUTPUT_FLAG_NONE,
    callback_t cbf = NULL,
    void* user = NULL,
    int32_t notificationFrames = 0,
    audio_session_t sessionId = AUDIO_SESSION_ALLOCATE,
    transfer_type transferType = TRANSFER_DEFAULT,
    const audio_offload_info_t *offloadInfo = NULL,
    const AttributionSourceState& attributionSource = AttributionSourceState(),
    const audio_attributes_t* pAttributes = NULL,
    bool doNotReconnect = false,
    float maxRequiredSpeed = 1.0f
)
```

**关键点**:
- `sharedBuffer` 参数是一个 `sp<IMemory>` 智能指针
- 应用层需要预先分配并填充音频数据到这个共享内存
- 通过 Binder 共享内存机制实现零拷贝传递

### 2. Transfer 模式自动识别

**文件**: `d:\code\Audio相关\framework\libaudioclient\AudioTrack.cpp` (行: 492-535)

```cpp
switch (transferType) {
case TRANSFER_DEFAULT:
    if (sharedBuffer != 0) {
        transferType = TRANSFER_SHARED;  // ← 自动设置为 TRANSFER_SHARED
    } else if (cbf == NULL || threadCanCallJava) {
        transferType = TRANSFER_SYNC;
    } else {
        transferType = TRANSFER_CALLBACK;
    }
    break;

case TRANSFER_SHARED:
    if (sharedBuffer == 0) {
        // 验证：TRANSFER_SHARED 模式必须有 sharedBuffer
        errorMessage = StringPrintf(
                "%s: Transfer type TRANSFER_SHARED but sharedBuffer == 0", __func__);
        status = BAD_VALUE;
        goto error;
    }
    break;
}

mSharedBuffer = sharedBuffer;  // ← 保存到成员变量
mTransfer = transferType;
```

**关键点**:
- 有 `sharedBuffer` 时自动设置 `mTransfer = TRANSFER_SHARED`
- `mSharedBuffer` 成员变量保存对共享内存的引用

### 3. createTrack_l() 创建 Static Proxy

**文件**: `d:\code\Audio相关\framework\libaudioclient\AudioTrack.cpp` (行: 1822-2070)

```cpp
status_t AudioTrack::createTrack_l()
{
    // ... 准备 input 参数
    IAudioFlinger::CreateTrackInput input;

    input.sharedBuffer = mSharedBuffer;  // ← 传递 sharedBuffer 到服务端
    input.flags = mFlags;
    input.frameCount = mReqFrameCount;
    // ...

    // 通过 Binder 调用服务端创建 track
    status = audioFlinger->createTrack(VALUE_OR_FATAL(input.toAidl()), response);

    // ... 获取控制块和缓冲区地址
    void* buffers;
    if (mSharedBuffer == 0) {
        // 动态模式：使用控制块后的缓冲区
        buffers = cblk + 1;
    } else {
        // Static 模式：使用 sharedBuffer 的数据
        buffers = mSharedBuffer->unsecurePointer();  // ← 直接使用 shared buffer
    }

    // 创建对应的 Proxy
    if (mSharedBuffer == 0) {
        // 动态模式
        mProxy = new AudioTrackClientProxy(cblk, buffers, mFrameCount, mFrameSize);
    } else {
        // Static 模式：创建 StaticAudioTrackClientProxy
        mStaticProxy = new StaticAudioTrackClientProxy(cblk, buffers,
                                                        mFrameCount, mFrameSize);
        mProxy = mStaticProxy;  // ← 使用 Static 专用 Proxy
    }
}
```

**StaticAudioTrackClientProxy 特有方法**:

```cpp
// 文件: d:\code\Audio相关\framework\libaudioclient\AudioTrackShared.cpp (行: 619-642)

// 设置循环播放参数
void StaticAudioTrackClientProxy::setLoop(size_t loopStart, size_t loopEnd, int loopCount)
{
    mState.mLoopStart = (uint32_t) loopStart;
    mState.mLoopEnd = (uint32_t) loopEnd;
    mState.mLoopCount = loopCount;
    mState.mLoopSequence++;
    mMutator.push(mState);
}

// 设置播放位置
void StaticAudioTrackClientProxy::setBufferPosition(size_t position)
{
    mState.mPosition = (uint32_t) position;
    mState.mPositionSequence++;
    mMutator.push(mState);
}

// Static 模式禁止 flush
void StaticAudioTrackClientProxy::flush()
{
    LOG_ALWAYS_FATAL("static flush");  // ← 不支持 flush
}
```

### 4. 服务端接收和处理 Static Buffer

**文件**: `d:\code\Audio相关\framework\audioflinger\Threads.cpp` (行: 2261-2592)

```cpp
sp<AudioFlinger::PlaybackThread::Track>
AudioFlinger::PlaybackThread::createTrack_l(
    ..., const sp<IMemory>& sharedBuffer, ...)
{
    // 验证 sharedBuffer
    if (sharedBuffer != 0 && checkIMemory(sharedBuffer) != NO_ERROR) {
        lStatus = BAD_VALUE;
        goto Exit;
    }

    // 对于 static buffer，frameCount 从 sharedBuffer 计算
    if (sharedBuffer != 0) {
        // 计算实际帧数
        frameCount = sharedBuffer->size() / frameSize;
    }

    // 创建 Track 对象
    track = new Track(this, client, streamType, attr, sampleRate, format,
                      channelMask, frameCount,
                      nullptr /* buffer */, (size_t)0 /* bufferSize */,
                      sharedBuffer,  // ← 传递到 Track
                      sessionId, creatorPid, uid,
                      *flags, TrackBase::TYPE_DEFAULT, portId,
                      SIZE_MAX, opPackageName);
}
```

**Track 构造函数存储 sharedBuffer**:

**文件**: `d:\code\Audio相关\framework\audioflinger\Tracks.cpp` (行: 509-586)

```cpp
AudioFlinger::PlaybackThread::Track::Track(
    ..., const sp<IMemory>& sharedBuffer, ...)
    : TrackBase(thread, client, attr, sampleRate, format, channelMask, frameCount,
                // 从 sharedBuffer 获取数据指针
                (sharedBuffer != 0) ? sharedBuffer->unsecurePointer() : buffer,
                (sharedBuffer != 0) ? sharedBuffer->size() : bufferSize,
                sessionId, creatorPid, uid, true /*isOut*/,
                ALLOC_CBLK, type, portId, ...),
      mSharedBuffer(sharedBuffer),  // ← 保存 sharedBuffer 引用
      // ...
{
    // 创建服务端 Proxy
    if (sharedBuffer == 0) {
        // 动态模式
        mAudioTrackServerProxy = new AudioTrackServerProxy(mCblk, mBuffer, frameCount,
                mFrameSize, !isExternalTrack(), sampleRate);
    } else {
        // Static 模式：创建 StaticAudioTrackServerProxy
        mAudioTrackServerProxy = new StaticAudioTrackServerProxy(mCblk, mBuffer, frameCount,
                mFrameSize, sampleRate);  // ← mBuffer 指向 sharedBuffer 数据
    }
    mServerProxy = mAudioTrackServerProxy;
}
```

---

## 数据一次性写入机制

### ❌ Static 模式禁止动态写入

**文件**: `d:\code\Audio相关\framework\libaudioclient\AudioTrack.cpp`

```cpp
// 1. Static 模式禁止调用 write()
ssize_t AudioTrack::write(const void* buffer, size_t userSize, bool blocking)
{
    if (mTransfer != TRANSFER_SYNC && mTransfer != TRANSFER_SYNC_NOTIF_CALLBACK) {
        return INVALID_OPERATION;  // ← TRANSFER_SHARED 模式返回错误
    }
    // ...
}

// 2. Static 模式禁止调用 releaseBuffer()
void AudioTrack::releaseBuffer(const Buffer* audioBuffer)
{
    if (mTransfer == TRANSFER_SHARED) {
        return;  // ← 直接返回，不做任何处理
    }
    // ...
}
```

### ✅ 正确的 Static 模式使用流程

```cpp
// 应用层代码示例

// 1. 分配共享内存（通过 MemoryDealer 或 ashmem）
sp<MemoryDealer> memoryDealer = new MemoryDealer(bufferSize, "AudioTrack");
sp<IMemory> sharedBuffer = memoryDealer->allocate(bufferSize);

// 2. 将完整的音频数据写入 IMemory
void* data = sharedBuffer->unsecurePointer();
memcpy(data, audioData, bufferSize);  // 一次性写入所有数据

// 3. 创建 AudioTrack 时传入已填充数据的 sharedBuffer
AudioTrack* track = new AudioTrack(
    AUDIO_STREAM_MUSIC,
    sampleRate,
    AUDIO_FORMAT_PCM_16_BIT,
    channelMask,
    sharedBuffer,  // ← 传入预填充的 shared buffer
    AUDIO_OUTPUT_FLAG_NONE
);

// 4. 调用 start() 开始播放（服务端直接从 sharedBuffer 读取数据）
track->start();
```

---

## PlaybackThread::threadLoop 处理逻辑

### 1. threadLoop 主循环

**文件**: `d:\code\Audio相关\framework\audioflinger\Threads.cpp` (行: 3956-4600)

```cpp
bool AudioFlinger::PlaybackThread::threadLoop()
{
    for (int64_t loopCount = 0; !exitPending(); ++loopCount) {

        // a) 准备音频轨道（检查数据可用性）
        mMixerStatus = prepareTracks_l(&tracksToRemove);

        // b) 检查是否需要进入待机 (standby)
        checkSilentMode_l();

        // c) 混音处理
        if (mMixerStatus == MIXER_TRACKS_READY) {
            threadLoop_mix();  // 将各轨道数据混音到 mSinkBuffer
        }

        // d) 音效处理
        threadLoop_effect();

        // e) 写入 HAL ★★★ 关键步骤 ★★★
        ret = threadLoop_write();

        // f) 性能统计和延迟管理
        threadLoop_sleepTime();
    }
}
```

### 2. threadLoop_write() 写入 HAL

**文件**: `d:\code\Audio相关\framework\audioflinger\Threads.cpp` (行: 3428-3558)

```cpp
ssize_t AudioFlinger::PlaybackThread::threadLoop_write()
{
    LOG_HIST_TS();
    mInWrite = true;
    ssize_t bytesWritten;
    const size_t offset = mCurrentWriteLength - mBytesRemaining;

    // 如果存在 NBAIO sink，使用它写入混音器的输出
    if (mNormalSink != 0) {
        const size_t count = mBytesRemaining / mFrameSize;

        // ★ 从 mSinkBuffer 写入数据（不区分 static 或 streaming）
        ssize_t framesWritten = mNormalSink->write(
            (char *)mSinkBuffer + offset, count);

        if (framesWritten > 0) {
            bytesWritten = framesWritten * mFrameSize;
        }
    }
    // 否则直接使用 HAL / AudioStreamOut
    else {
        // Direct output 和 offload 线程
        if (mUseAsyncWrite) {
            // 异步写入逻辑
            mWriteAckSequence += 2;
            mWriteAckSequence |= 1;
            callback->setWriteBlocked(mWriteAckSequence);
        }

        // ★ 直接写入 HAL
        bytesWritten = mOutput->write((char *)mSinkBuffer + offset, mBytesRemaining);
    }

    mNumWrites++;
    mInWrite = false;
    mStandby = false;
    return bytesWritten;
}
```

**关键点**:
- ✅ **不区分 static buffer 和普通 buffer**
- ✅ 统一从 `mSinkBuffer` 读取混音后的数据写入 HAL
- ✅ Static buffer 的特殊处理在**更早的阶段**（`prepareTracks_l` 和 `getNextBuffer`）

### 3. prepareTracks_l() 处理 Static Track

**文件**: `d:\code\Audio相关\framework\audioflinger\Threads.cpp` (行: 5551-6350)

#### Static Buffer 判断

```cpp
// 第 5987 行：判断是否为 static buffer
if ((track->sharedBuffer() == 0) && !track->isStopped() && !track->isPausing() &&
    (mMixerStatusIgnoringFastTracks == MIXER_TRACKS_READY)) {
    minFrames = desiredFrames;  // 普通 buffer 需要足够的数据
}
// Static buffer: minFrames = 1（只要有数据就可以播放）
```

#### 下溢处理差异

```cpp
// 第 5798 行：快速轨道特殊处理
if (track->sharedBuffer() == 0) {  // 非 static buffer 轨道
    // 检查"空"下溢并递减重试计数
    if (recentEmpty == 0) {
        break;  // 忽略部分下溢
    }
    if (--(track->mRetryCount) > 0) {
        break;
    }
    track->disable();
    isActive = false;
}
// Static buffer 轨道会执行 FALLTHROUGH_INTENDED 继续处理
```

#### 播放完成检查

```cpp
// 行 5838
if (!(mStandby || track->presentationComplete(framesWritten, audioHALFrames))) {
    break;  // 轨道保持在活跃列表中，直到播放完成
}
```

### 4. Track::getNextBuffer() 获取数据

**文件**: `d:\code\Audio相关\framework\audioflinger\Tracks.cpp` (行: 815-831)

```cpp
status_t AudioFlinger::PlaybackThread::Track::getNextBuffer(
        AudioBufferProvider::Buffer* buffer)
{
    ServerProxy::Buffer buf;
    size_t desiredFrames = buffer->frameCount;
    buf.mFrameCount = desiredFrames;

    // ★ 调用 ServerProxy 获取缓冲区（多态调用）
    status_t status = mServerProxy->obtainBuffer(&buf);

    buffer->frameCount = buf.mFrameCount;
    buffer->raw = buf.mRaw;  // ← 指向实际数据的指针

    // 下溢统计
    if (buf.mFrameCount == 0 && !isStopping() && !isStopped() && !isPaused() && !isOffloaded()) {
        mAudioTrackServerProxy->tallyUnderrunFrames(desiredFrames);
    }

    return status;
}
```

#### StaticAudioTrackServerProxy::obtainBuffer() 实现

**文件**: `d:\code\Audio相关\framework\libaudioclient\AudioTrackShared.cpp` (行: 1146-1190)

```cpp
status_t StaticAudioTrackServerProxy::obtainBuffer(Buffer* buffer, bool ackFlush)
{
    if (mIsShutdown) {
        buffer->mFrameCount = 0;
        buffer->mRaw = NULL;
        return NO_INIT;
    }

    // 1. 轮询位置和循环状态
    ssize_t positionOrStatus = pollPosition();
    if (positionOrStatus < 0) {
        buffer->mFrameCount = 0;
        return (status_t) positionOrStatus;
    }

    size_t position = (size_t) positionOrStatus;

    // 2. 确定结束位置：如果有循环则为 loopEnd，否则为 frameCount
    size_t end = mState.mLoopCount != 0 ? mState.mLoopEnd : mFrameCount;

    // 3. 计算可用帧数
    size_t avail;
    if (position < end) {
        avail = end - position;
        size_t wanted = buffer->mFrameCount;
        if (avail < wanted) {
            buffer->mFrameCount = avail;
        } else {
            avail = wanted;
        }

        // 4. ★★★ 关键：直接返回 shared buffer 中的指针（零拷贝）★★★
        buffer->mRaw = &((char *) mBuffers)[position * mFrameSize];
    } else {
        // 已到末尾，没有可用数据
        avail = 0;
        buffer->mFrameCount = 0;
        buffer->mRaw = NULL;
    }

    // 5. 计算剩余非连续帧数
    buffer->mNonContig = mFramesReady == INT64_MAX ? SIZE_MAX :
                         clampToSize(mFramesReady - avail);

    if (!ackFlush) {
        mUnreleased = avail;
    }
    return NO_ERROR;
}
```

**关键点**:
- ✅ **零拷贝**: `buffer->mRaw` 直接指向 shared memory 中的数据
- ✅ 支持循环播放：根据 `mLoopCount` 决定结束位置
- ✅ 位置追踪：通过 `pollPosition()` 更新播放位置

---

## 数据从 Track 到 HAL 的流转路径

### 完整数据流程图

```
┌──────────────────────────────────────────────────────────────────┐
│                        客户端流程（应用层）                         │
├──────────────────────────────────────────────────────────────────┤
│ 1. 应用分配 sp<IMemory> sharedBuffer                              │
│    └─> MemoryDealer::allocate(bufferSize)                       │
│                                                                  │
│ 2. 应用将完整音频数据写入 sharedBuffer                            │
│    └─> memcpy(sharedBuffer->unsecurePointer(), audioData, size) │
│                                                                  │
│ 3. 创建 AudioTrack(sharedBuffer)                                 │
│    └─> set() 自动设置 mTransfer = TRANSFER_SHARED               │
│    └─> mSharedBuffer = sharedBuffer                             │
│                                                                  │
│ 4. createTrack_l()                                              │
│    └─> buffers = mSharedBuffer->unsecurePointer()               │
│    └─> mStaticProxy = new StaticAudioTrackClientProxy(...)      │
│                                                                  │
│ 5. start() 开始播放                                              │
│    └─> mAudioTrack->start(&status)                              │
└──────────────────────────────────────────────────────────────────┘
                            ↓ (Binder IPC)
┌──────────────────────────────────────────────────────────────────┐
│                   服务端流程（AudioFlinger）                       │
├──────────────────────────────────────────────────────────────────┤
│ 1. AudioFlinger::createTrack(input.sharedBuffer)                 │
│                                                                  │
│ 2. PlaybackThread::createTrack_l(sharedBuffer)                   │
│    └─> frameCount = sharedBuffer->size() / frameSize            │
│    └─> track = new Track(..., sharedBuffer)                     │
│                                                                  │
│ 3. Track::Track(sharedBuffer)                                    │
│    └─> mBuffer = sharedBuffer->unsecurePointer()                │
│    └─> mSharedBuffer = sharedBuffer                             │
│    └─> mAudioTrackServerProxy =                                 │
│        new StaticAudioTrackServerProxy(mCblk, mBuffer, ...)      │
│                                                                  │
│ 4. PlaybackThread::threadLoop() 主循环                           │
│    │                                                             │
│    ├─> prepareTracks_l()  // 准备轨道                            │
│    │   ├─> track->framesReady()                                 │
│    │   │   └─> StaticProxy::framesReady()                       │
│    │   │       └─> pollPosition() // 更新位置和循环状态          │
│    │   │           └─> 计算 mFramesReady                        │
│    │   │                                                         │
│    │   └─> mAudioMixer->setBufferProvider(trackId, track)       │
│    │       // 将 track 设置为混音器的数据提供者                  │
│    │                                                             │
│    ├─> threadLoop_mix()  // 混音处理                            │
│    │   └─> AudioMixer::process()                                │
│    │       ├─> track->getNextBuffer(&buffer)  // 多次调用        │
│    │       │   └─> StaticProxy::obtainBuffer()                  │
│    │       │       ├─> pollPosition() // 更新循环状态            │
│    │       │       ├─> 计算 position 和 end                     │
│    │       │       ├─> buffer->mRaw = &mBuffers[position * ...] │
│    │       │       │   // ★ 直接返回 shared buffer 指针 ★       │
│    │       │       └─> buffer->mFrameCount = 可用帧数           │
│    │       │                                                     │
│    │       ├─> 从 buffer->mRaw 读取数据                          │
│    │       │   混音到 mMixerBuffer / mSinkBuffer                 │
│    │       │                                                     │
│    │       └─> track->releaseBuffer(&buffer)                    │
│    │           └─> StaticProxy::releaseBuffer()                 │
│    │               ├─> newPosition = position + stepCount       │
│    │               ├─> if (newPosition == mLoopEnd) {           │
│    │               │       newPosition = mLoopStart; // 循环     │
│    │               │       mLoopCount--;                        │
│    │               │       setFlags |= CBLK_LOOP_CYCLE;         │
│    │               │   }                                         │
│    │               ├─> if (newPosition == mFrameCount)          │
│    │               │       setFlags |= CBLK_BUFFER_END;         │
│    │               ├─> mState.mPosition = newPosition           │
│    │               └─> 更新 mFramesReady                        │
│    │                                                             │
│    └─> threadLoop_write()  // 写入 HAL                          │
│        └─> mNormalSink->write(mSinkBuffer, count)               │
│            或 mOutput->write(mSinkBuffer, count)                 │
│            // 从混音后的 sink buffer 写入硬件                     │
└──────────────────────────────────────────────────────────────────┘
```

### 关键差异对比表

| 步骤 | 普通 Buffer (Streaming) | Static Buffer |
|------|------------------------|---------------|
| **内存来源** | AudioFlinger 分配 | 应用层提供 IMemory |
| **数据填充** | 应用调用 write() 持续填充 | 创建前一次性填充 |
| **obtainBuffer** | 从环形缓冲区读取，需要 wrap 处理 | 直接返回 shared buffer 指针 |
| **releaseBuffer** | 更新读指针，检查下溢 | 更新位置，处理循环逻辑 |
| **framesReady** | 环形缓冲区可用帧数 | 考虑循环次数的剩余帧数 |
| **数据拷贝** | ✅ 需要拷贝到混音缓冲区 | ⚡ 零拷贝（直接从 shared buffer）|
| **循环播放** | ❌ 不支持 | ✅ 支持循环参数 |

---

## 播放完成控制机制

### 1. presentationComplete() 判断播放完成

**文件**: `d:\code\Audio相关\framework\audioflinger\Tracks.cpp` (行: 1327-1367)

```cpp
bool AudioFlinger::PlaybackThread::Track::presentationComplete(
        int64_t framesWritten, size_t audioHalFrames)
{
    // 核心判断逻辑：
    // 当第一次调用时，记录完成帧数 = 当前已写入帧数 + HAL缓冲区帧数
    if (mPresentationCompleteFrames == 0) {
        mPresentationCompleteFrames = framesWritten + audioHalFrames;
    }

    bool complete;
    if (isOffloaded()) {
        complete = true;  // Offload轨道直接返回true
    } else if (isDirect() || isFastTrack()) {
        // Direct和Fast轨道：比较已写入帧数是否达到目标
        complete = framesWritten >= (int64_t) mPresentationCompleteFrames;
    } else {
        // 普通轨道（包括static track）：需要同时满足两个条件
        complete = framesWritten >= (int64_t) mPresentationCompleteFrames
                && mAudioTrackServerProxy->isDrained();  // ← 检查缓冲区是否排空
    }

    if (complete) {
        // 播放完成时的操作
        triggerEvents(AudioSystem::SYNC_EVENT_PRESENTATION_COMPLETE);
        mAudioTrackServerProxy->setStreamEndDone();  // ← 设置流结束标志
        return true;
    }
    return false;
}
```

**判断逻辑**:
1. ✅ 检查已写入 HAL 的帧数是否达到预期
2. ✅ 检查 ServerProxy 缓冲区是否已排空（`isDrained()`）
3. ✅ 两个条件同时满足才认为播放完成

### 2. CBLK 标志位机制

#### 标志位定义

```cpp
// 文件: d:\code\Audio相关\include\private\media\AudioTrackShared.h

#define CBLK_UNDERRUN   0x01  // 下溢
#define CBLK_LOOP_CYCLE 0x02  // 循环中（完成一次循环）
#define CBLK_LOOP_FINAL 0x04  // 最后一次循环
#define CBLK_BUFFER_END 0x08  // 缓冲区结束
#define CBLK_DISABLED   0x10  // 轨道被禁用
#define CBLK_STREAM_END_DONE 0x20  // 流结束完成
```

#### 标志位设置

**文件**: `d:\code\Audio相关\framework\libaudioclient\AudioTrackShared.cpp` (行: 1214-1245)

```cpp
void StaticAudioTrackServerProxy::releaseBuffer(Buffer* buffer)
{
    size_t stepCount = buffer->mFrameCount;
    size_t position = mState.mPosition;
    size_t newPosition = position + stepCount;
    int32_t setFlags = 0;

    // 判断是否到达循环结束点
    if (mState.mLoopCount != 0 && newPosition == mState.mLoopEnd) {
        newPosition = mState.mLoopStart;  // 跳回循环起点

        if (mState.mLoopCount == -1 || --mState.mLoopCount != 0) {
            setFlags = CBLK_LOOP_CYCLE;      // ← 循环继续
        } else {
            setFlags = CBLK_LOOP_FINAL;      // ← 最后一次循环
        }
    }

    // 判断是否到达缓冲区结束
    if (newPosition == mFrameCount) {
        setFlags |= CBLK_BUFFER_END;          // ← 缓冲区播放完成
    }

    mState.mPosition = newPosition;

    // 更新剩余帧数
    if (mFramesReady != INT64_MAX) {
        mFramesReady -= stepCount;
    }
    mFramesReadySafe = clampToSize(mFramesReady);

    // ★ 通过共享内存通知客户端 ★
    if (setFlags != 0) {
        android_atomic_or(setFlags, &cblk->mFlags);
    }
}
```

#### 客户端读取标志位

**文件**: `d:\code\Audio相关\framework\libaudioclient\AudioTrack.cpp` (行: 2483-2644)

```cpp
nsecs_t AudioTrack::processAudioBuffer()
{
    // 清除标志位（原子操作）
    int32_t flags = android_atomic_and(
        ~(CBLK_UNDERRUN | CBLK_LOOP_CYCLE | CBLK_LOOP_FINAL | CBLK_BUFFER_END),
        &mCblk->mFlags);

    // 处理 BUFFER_END 事件
    if (flags & CBLK_BUFFER_END) {
        mCbf(EVENT_BUFFER_END, mUserData, NULL);  // ← 回调应用层
    }

    // 处理循环事件
    loopCountNotifications = int((flags & (CBLK_LOOP_CYCLE | CBLK_LOOP_FINAL)) != 0);
    while (loopCountNotifications > 0) {
        mCbf(EVENT_LOOP_END, mUserData, NULL);  // ← 回调应用层
        --loopCountNotifications;
    }
}
```

### 3. framesReady() 返回 0 停止混音

**文件**: `d:\code\Audio相关\framework\audioflinger\Tracks.cpp` (行: 872-878)

```cpp
size_t AudioFlinger::PlaybackThread::Track::framesReady() const {
    if (mSharedBuffer != 0 && (isStopped() || isStopping())) {
        // ★ Static tracks 在停止时立即返回零帧 ★
        // 缓冲区剩余部分不会被消费
        return 0;
    }
    return mAudioTrackServerProxy->framesReady();
}
```

**效果**:
- ✅ `framesReady()` 返回 0
- ✅ `prepareTracks_l()` 检测到无可用帧
- ✅ 轨道不再被加入混音器
- ✅ `threadLoop_write()` 不再写入此轨道的数据到 HAL

### 4. Static Buffer 状态机

```
┌─────────────────────────────────────────────────────────────┐
│                  Static Track 状态机                          │
└─────────────────────────────────────────────────────────────┘

1. 初始状态：
   IDLE/STOPPED → start() → ACTIVE (FS_FILLING)

2. 填充状态：
   FS_FILLING → (framesReady >= threshold) → FS_FILLED → FS_ACTIVE

3. 播放过程（Fast Track）：
   ┌──────────────────────────────────────────────┐
   │ ACTIVE → (缓冲区数据读完)                      │
   │   ↓                                           │
   │ STOPPING_1 → (检测到underrun)                 │
   │   ↓                                           │
   │ STOPPING_2 → (presentationComplete == true)  │
   │   ↓                                           │
   │ STOPPED                                       │
   └──────────────────────────────────────────────┘

4. 播放过程（Normal Track）：
   ACTIVE → stop() → STOPPED (立即停止)

5. 循环播放：
   • mLoopCount > 0: 播放 N 次后设置 CBLK_LOOP_FINAL → STOPPING
   • mLoopCount == -1: 无限循环，直到显式 stop()
   • 每次循环设置 CBLK_LOOP_CYCLE 标志

6. 缓冲区结束：
   position == mFrameCount → 设置 CBLK_BUFFER_END → EVENT_BUFFER_END
```

**状态判断方法** (`TrackBase.h`):

```cpp
bool isStopped() const {
    return (mState == STOPPED || mState == FLUSHED);
}

bool isStopping() const {
    return mState == STOPPING_1 || mState == STOPPING_2;
}
```

### 5. 播放完成后的清理

**文件**: `d:\code\Audio相关\framework\audioflinger\Threads.cpp` (行: 6242-6258)

```cpp
// 对于 static track，检查是否播放完成
if ((track->sharedBuffer() != 0) || track->isTerminated() ||
    track->isStopped() || track->isPaused())
{
    // 已消费所有缓冲区数据
    size_t audioHALFrames = (latency_l() * mSampleRate) / 1000;
    int64_t framesWritten = mBytesWritten / mFrameSize;

    if (mStandby || track->presentationComplete(framesWritten, audioHALFrames))
    {
        if (track->isStopped()) {
            track->reset();  // ← 重置轨道状态
        }
        tracksToRemove->add(track);  // ← 从活跃列表移除
    }
}
```

**清理流程**:
1. ✅ 检查 `presentationComplete()` 确认播放完成
2. ✅ 调用 `track->reset()` 重置内部状态
3. ✅ 从 `mActiveTracks` 移除轨道
4. ✅ 不再调用 `getNextBuffer()` 获取数据
5. ✅ `threadLoop_write()` 不再写入此轨道数据到 HAL

---

## 循环播放控制

### 1. 设置循环参数

**文件**: `d:\code\Audio相关\framework\libaudioclient\AudioTrack.cpp` (行: 1510-1545)

```cpp
status_t AudioTrack::setLoop(uint32_t loopStart, uint32_t loopEnd, int loopCount)
{
    // 只能用于 static buffer
    if (mSharedBuffer == 0 || isOffloadedOrDirect()) {
        return INVALID_OPERATION;
    }

    // 验证参数
    if (loopCount == 0) {
        // 禁用循环
    } else if (loopCount >= -1 && loopStart < loopEnd && loopEnd <= mFrameCount &&
               loopEnd - loopStart >= MIN_LOOP) {
        // 有效的循环参数
    } else {
        return BAD_VALUE;
    }

    AutoMutex lock(mLock);
    if (mState == STATE_ACTIVE) {
        return INVALID_OPERATION;  // 不能在播放时设置
    }
    setLoop_l(loopStart, loopEnd, loopCount);
    return NO_ERROR;
}

void AudioTrack::setLoop_l(uint32_t loopStart, uint32_t loopEnd, int loopCount)
{
    mLoopCount = loopCount;
    mLoopEnd = loopEnd;
    mLoopStart = loopStart;
    mLoopCountNotified = loopCount;

    // ★ 通知客户端 Proxy ★
    mStaticProxy->setLoop(loopStart, loopEnd, loopCount);
}
```

**循环参数说明**:
- `loopStart`: 循环起始帧位置
- `loopEnd`: 循环结束帧位置
- `loopCount`:
  - `0`: 禁用循环
  - `> 0`: 循环次数（播放 N 次）
  - `-1`: 无限循环

### 2. 循环状态管理

**文件**: `d:\code\Audio相关\framework\libaudioclient\AudioTrackShared.cpp` (行: 1193-1252)

```cpp
void StaticAudioTrackServerProxy::releaseBuffer(Buffer* buffer)
{
    size_t stepCount = buffer->mFrameCount;
    size_t position = mState.mPosition;
    size_t newPosition = position + stepCount;
    int32_t setFlags = 0;

    // ★★★ 循环逻辑核心 ★★★
    if (mState.mLoopCount != 0 && newPosition == mState.mLoopEnd) {
        // 到达循环结束点
        newPosition = mState.mLoopStart;  // 跳回循环起始点

        if (mState.mLoopCount == -1) {
            // 无限循环
            setFlags = CBLK_LOOP_CYCLE;
        } else if (--mState.mLoopCount != 0) {
            // 还有剩余循环次数
            setFlags = CBLK_LOOP_CYCLE;
        } else {
            // 最后一次循环完成
            setFlags = CBLK_LOOP_FINAL;
        }
    }

    // 检查是否到达缓冲区末尾
    if (newPosition == mFrameCount) {
        setFlags |= CBLK_BUFFER_END;
    }

    mState.mPosition = newPosition;

    // 更新剩余帧数
    if (mFramesReady != INT64_MAX) {
        mFramesReady -= stepCount;
    }
    mFramesReadySafe = clampToSize(mFramesReady);

    // 更新服务器位置
    cblk->mServer += stepCount;
    mReleased += stepCount;

    // ★ 推送位置和循环计数到客户端 ★
    StaticAudioTrackPosLoop posLoop;
    posLoop.mBufferPosition = mState.mPosition;
    posLoop.mLoopCount = mState.mLoopCount;
    mPosLoopMutator.push(posLoop);

    // ★ 设置标志位通知客户端 ★
    if (setFlags != 0) {
        android_atomic_or(setFlags, &cblk->mFlags);
    }
}
```

### 3. 剩余帧数计算

**文件**: `d:\code\Audio相关\framework\libaudioclient\AudioTrackShared.cpp` (行: 1123-1133)

```cpp
// 根据循环参数计算剩余帧数
if (mState.mLoopCount == -1) {
    // 无限循环
    mFramesReady = INT64_MAX;
} else if (mState.mLoopCount == 0) {
    // 无循环，直到结束
    mFramesReady = mFrameCount - mState.mPosition;
} else if (mState.mLoopCount > 0) {
    // 有限次循环：循环次数 × 循环长度 + 剩余到结束的帧数
    mFramesReady = int64_t(mState.mLoopCount) * (mState.mLoopEnd - mState.mLoopStart)
                   + mFrameCount - mState.mPosition;
}
mFramesReadySafe = clampToSize(mFramesReady);
```

### 4. 循环播放流程图

```
┌───────────────────────────────────────────────────────────────┐
│                      循环播放流程                               │
└───────────────────────────────────────────────────────────────┘

初始状态：position = 0, loopCount = 3, loopStart = 100, loopEnd = 500

播放流程：
┌──────┐
│ 开始  │ position = 0
└──┬───┘
   │
   v
┌──────────────────────────────┐
│ 播放前导部分 (0 → 100)         │
└──────────┬───────────────────┘
           │
           v
┌──────────────────────────────┐  loopCount = 3
│ 第 1 次循环 (100 → 500)       │  position: 100 → 500
│   → CBLK_LOOP_CYCLE          │  loopCount--: 3 → 2
└──────────┬───────────────────┘  newPosition = loopStart (100)
           │
           v
┌──────────────────────────────┐  loopCount = 2
│ 第 2 次循环 (100 → 500)       │  position: 100 → 500
│   → CBLK_LOOP_CYCLE          │  loopCount--: 2 → 1
└──────────┬───────────────────┘  newPosition = loopStart (100)
           │
           v
┌──────────────────────────────┐  loopCount = 1
│ 第 3 次循环 (100 → 500)       │  position: 100 → 500
│   → CBLK_LOOP_FINAL          │  loopCount--: 1 → 0
└──────────┬───────────────────┘  newPosition = loopStart (100)
           │                      但由于 loopCount = 0，不再循环
           v
┌──────────────────────────────┐
│ 播放尾部 (100 → frameCount)   │  position: 100 → frameCount
│   → CBLK_BUFFER_END          │
└──────────┬───────────────────┘
           │
           v
      ┌────────┐
      │ 播放完成 │
      └────────┘
```

---

## 关键代码位置索引

### 客户端 (AudioTrack.cpp)

| 功能 | 文件路径 | 行号 |
|------|---------|------|
| **构造函数（sharedBuffer 参数）** | `d:\code\Audio相关\framework\libaudioclient\AudioTrack.cpp` | 308-324 |
| **Transfer 模式识别** | 同上 | 492-535 |
| **createTrack_l()** | 同上 | 1822-2070 |
| **Static Proxy 创建** | 同上 | 2064-2070 |
| **禁止 write()** | 同上 | 2372-2377 |
| **禁止 releaseBuffer()** | 同上 | 2327-2332 |
| **setLoop() 设置循环** | 同上 | 1510-1545 |
| **CBLK 标志位读取** | 同上 | 2483-2644 |

### 服务端 Proxy (AudioTrackShared.cpp)

| 功能 | 文件路径 | 行号 |
|------|---------|------|
| **StaticAudioTrackClientProxy 构造** | `d:\code\Audio相关\framework\libaudioclient\AudioTrackShared.cpp` | 599-607 |
| **setLoop() 实现** | 同上 | 619-630 |
| **setBufferPosition()** | 同上 | 632-642 |
| **StaticAudioTrackServerProxy** | 同上 | 1021-1272 |
| **obtainBuffer() 实现** | 同上 | 1146-1190 |
| **releaseBuffer() 循环处理** | 同上 | 1193-1252 |
| **pollPosition()** | 同上 | 1100-1142 |
| **剩余帧数计算** | 同上 | 1123-1133 |

### AudioFlinger (Threads.cpp)

| 功能 | 文件路径 | 行号 |
|------|---------|------|
| **createTrack_l()** | `d:\code\Audio相关\framework\audioflinger\Threads.cpp` | 2261-2592 |
| **threadLoop() 主循环** | 同上 | 3956-4600 |
| **threadLoop_write()** | 同上 | 3428-3558 |
| **prepareTracks_l()** | 同上 | 5551-6350 |
| **Static buffer 判断** | 同上 | 5987 |
| **下溢处理差异** | 同上 | 5798-5810 |
| **播放完成检查** | 同上 | 6242-6258 |

### Track (Tracks.cpp)

| 功能 | 文件路径 | 行号 |
|------|---------|------|
| **Track 构造函数** | `d:\code\Audio相关\framework\audioflinger\Tracks.cpp` | 509-586 |
| **StaticProxy 创建** | 同上 | 580-586 |
| **getNextBuffer()** | 同上 | 815-831 |
| **framesReady()** | 同上 | 872-878 |
| **presentationComplete()** | 同上 | 1327-1367 |

---

## 总结

### Static 模式的核心特点

1. **一次性数据加载**
   - 应用层在创建 AudioTrack 前，将完整音频数据写入 `sp<IMemory>` 共享内存
   - 创建 AudioTrack 时传入 `sharedBuffer`，自动设置为 `TRANSFER_SHARED` 模式
   - 禁止调用 `write()` 和 `releaseBuffer()` 动态填充数据

2. **零拷贝数据传递**
   - 通过 Binder 共享内存机制，客户端和服务端访问同一块物理内存
   - `StaticAudioTrackServerProxy::obtainBuffer()` 直接返回 shared buffer 指针
   - 避免了数据从应用层到 AudioFlinger 的拷贝开销

3. **专用 Proxy 实现**
   - 客户端: `StaticAudioTrackClientProxy`（管理循环参数、位置设置）
   - 服务端: `StaticAudioTrackServerProxy`（处理循环逻辑、位置追踪）
   - 支持循环播放（loopStart, loopEnd, loopCount）

4. **threadLoop 统一处理**
   - `threadLoop_write()` 不区分 static 或 streaming 模式
   - 统一从 `mSinkBuffer` 写入混音后的数据到 HAL
   - Static 特殊逻辑在 `prepareTracks_l()` 和 `getNextBuffer()` 中处理

5. **精确的播放完成控制**
   - `StaticAudioTrackServerProxy::releaseBuffer()` 更新位置和循环状态
   - 设置 CBLK 标志位（`CBLK_LOOP_CYCLE`, `CBLK_LOOP_FINAL`, `CBLK_BUFFER_END`）
   - `framesReady()` 在 stopped/stopping 状态返回 0，停止混音
   - `presentationComplete()` 检查 HAL 缓冲区排空，确认真正播放完成
   - 从 `mActiveTracks` 移除，不再写入 HAL

### 适用场景

✅ **适合 Static 模式的场景**:
- 短音效（按键音、提示音、通知音）
- 需要循环播放的音频（游戏背景音乐、环境音）
- 音频数据较小，可一次性加载到内存
- 需要低延迟、高性能播放

❌ **不适合 Static 模式的场景**:
- 长音频文件（音乐、视频音轨）
- 流媒体播放（需要边下载边播放）
- 需要动态调整播放内容（如实时音频处理）

### 性能优势

| 特性 | Streaming 模式 | Static 模式 | 优势 |
|------|---------------|-------------|------|
| **数据拷贝** | 多次拷贝（应用→AudioTrack→AudioFlinger） | 零拷贝（共享内存） | ⚡ 低 CPU 占用 |
| **延迟** | 需要缓冲区填充时间 | 数据已预加载 | ⚡ 低延迟 |
| **循环播放** | 需要应用层处理 | 框架原生支持 | ⚡ 简化应用逻辑 |
| **内存效率** | 环形缓冲区，适合大文件 | 一次性加载，适合小文件 | 各有优劣 |

---

**文档结束**

> 💡 **提示**: 本文档基于 Android Audio Framework 源码分析，版本可能随系统更新而变化。实际使用时请参考对应 Android 版本的源码。
