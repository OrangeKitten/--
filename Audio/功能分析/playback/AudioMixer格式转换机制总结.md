# AudioMixer 格式转换机制总结

## 问题 1：多格式 AudioTrack 的混音处理

### 提问
AudioFlinger 中的一个 mix 输出流支持 PCM 16bit 和 32bit。现在有两个 AudioTrack 使用这个输出流，一个是 16bit，一个是 32bit。那么其中一个输出流肯定是要做格式转换，然后再 mix 在一起。那么转换哪一个？为什么选择这个？规则是什么？

### 结论

**不是选择转换其中一个 Track，而是所有 Track 都会被转换到统一的 Mixer 内部格式（FLOAT）**。

#### 核心机制

1. **Mixer 内部格式是固定的**
   - `mMixerInFormat` 在创建时就确定了，通常是 `AUDIO_FORMAT_PCM_FLOAT`
   - 这是 Mixer 内部处理音频的统一格式

2. **每个 Track 都会做格式转换**
   - Track A (16bit) → ReformatBufferProvider → FLOAT
   - Track B (32bit) → ReformatBufferProvider → FLOAT
   - 然后在 FLOAT 格式下进行混音

3. **转换规则**
   - 不是选择转换其中一个 Track
   - **所有 Track 都转换到统一的 Mixer 内部格式（FLOAT）**
   - 混音后再转换为输出格式

#### 数据处理流程

```
Track A (16bit) → ReformatBufferProvider (16bit → FLOAT) → Mixer (FLOAT 混音)
Track B (32bit) → ReformatBufferProvider (32bit → FLOAT) → Mixer (FLOAT 混音)
                                                           ↓
                                                    混音结果 (FLOAT)
                                                           ↓
                                            convertMixerFormat (FLOAT → 输出格式)
                                                           ↓
                                                      AudioHAL
```

#### 为什么选择这个规则？

1. **精度优势**
   - FLOAT 格式（32bit 浮点）提供更高的动态范围和精度
   - 混音过程中不会因为整数运算导致精度损失
   - 避免溢出问题

2. **统一处理**
   - 所有 Track 转换到统一格式，简化混音算法
   - 不需要为不同格式组合编写多套混音代码

3. **性能考虑**
   - 现代 CPU 的浮点运算性能很好
   - 统一使用 FLOAT 可以利用 SIMD 指令优化

4. **灵活性**
   - 支持任意输入格式（8bit、16bit、24bit、32bit、FLOAT）
   - 输出也可以是任意格式
   - 中间统一用 FLOAT 处理

#### 关键代码位置

- **Mixer 内部格式设置**：`AudioMixerBase.cpp:149-150`
- **格式转换实现**：`AudioMixer.cpp:216-253` (prepareForReformat)
- **BufferProvider 链式结构**：`AudioMixer.cpp:319-347` (reconfigureBufferProviders)

---

## 问题 2：输出格式的决定因素

### 提问
最后到 AudioHAL 的音频格式跟 device 没有关系，而是根据 mixPort 中的能力定的。

### 结论

**完全正确！最终到 AudioHAL 的格式是根据 mixPort 配置决定的，而不是根据 device 配置。**

#### 核心机制

1. **配置文件中的定义**
   - `audio_policy_configuration.xml` 中定义了 mixPort 和 devicePort
   - mixPort 定义了混音器的能力（支持的格式、采样率、通道）
   - devicePort 定义了设备的能力

2. **格式选择流程**
   - AudioPolicyManager 调用 `getProfileForOutput` 遍历所有 HwModule 的 OutputProfiles（即 mixPort）
   - 调用 `IOProfile::isCompatibleProfile` 检查是否与 mixPort 配置兼容
   - 对于播放线程，必须**精确匹配** mixPort 的 profile（调用 `checkExactAudioProfile`）
   - 选择匹配的 IOProfile 后，AudioFlinger 根据 mixPort 配置创建输出流

3. **为什么这样设计？**
   - **解耦设备和混音器**：mixPort 代表混音器的能力，devicePort 代表设备的能力
   - **灵活的路由**：同一个 mixPort 可以路由到多个 device
   - **简化 AudioFlinger**：AudioFlinger 只需要关心 mixPort 的配置，不需要根据不同的 device 动态调整格式

#### 关键代码位置

- **IOProfile 兼容性检查**：`IOProfile.cpp:27-107` (isCompatibleProfile)
- **精确匹配检查**：`PolicyAudioPort.cpp:64-81` (checkExactAudioProfile)
- **Profile 选择**：`AudioPolicyManager.cpp:843-902` (getProfileForOutput)

---

## 问题 3：mixPort 支持多种格式时的输出格式

### 提问
mixPort 支持 16bit 和 32bit，那么都转换成 FLOAT 后 mix 在一起，最后送给 AudioHAL 的格式是什么样子的？

### 结论

**当 mixPort 支持多种格式时，系统会自动选择优先级最高的格式作为输出格式。**

#### 格式优先级表

```cpp
const audio_format_t PolicyAudioPort::sPcmFormatCompareTable[] = {
    AUDIO_FORMAT_DEFAULT,           // 优先级 0（最低）
    AUDIO_FORMAT_PCM_16_BIT,        // 优先级 1
    AUDIO_FORMAT_PCM_8_24_BIT,      // 优先级 2
    AUDIO_FORMAT_PCM_24_BIT_PACKED, // 优先级 3
    AUDIO_FORMAT_PCM_32_BIT,        // 优先级 4
    AUDIO_FORMAT_PCM_FLOAT,         // 优先级 5（最高）
};
```

#### 格式选择机制

1. **创建 AudioOutputDescriptor 时**
   - 调用 `pickAudioProfile` 从 mixPort 的多个 profile 中选择格式
   - 遍历所有 audio profiles，使用 `compareFormats` 比较格式优先级
   - 选择优先级最高的格式（但不超过 bestFormat）

2. **如果 mixPort 支持 16bit 和 32bit**
   - 系统会选择 **32bit**（优先级 4 > 优先级 1）
   - AudioOutputDescriptor 的 mFormat = 32bit

3. **完整的数据流**
   ```
   Track A (16bit) → Reformat (16bit → FLOAT) → Mixer (FLOAT 混音)
   Track B (32bit) → Reformat (32bit → FLOAT) → Mixer (FLOAT 混音)
                                                      ↓
                                               混音结果 (FLOAT)
                                                      ↓
                                   convertMixerFormat (FLOAT → 32bit)
                                                      ↓
                                                AudioHAL (32bit)
   ```

#### 为什么选择高优先级格式？

1. **精度保证**：选择更高位深度可以保留更多音频细节
2. **避免降质**：如果选择 16bit，32bit 的 Track 会损失精度
3. **统一处理**：所有 Track 都向上转换，不会有降质问题

#### 关键代码位置

- **格式优先级表**：`PolicyAudioPort.cpp:164-171` (sPcmFormatCompareTable)
- **格式比较函数**：`PolicyAudioPort.cpp:173-201` (compareFormats)
- **格式选择函数**：`PolicyAudioPort.cpp:224-267` (pickAudioProfile)
- **AudioOutputDescriptor 创建**：`AudioOutputDescriptor.cpp:41-51`

---

## 问题 4：播放 16bit 数据时的处理

### 提问
现在 mixPort 支持 16bit 和 32bit，根据之前的回答会在开机创建 32bit 的 AudioOutputDescriptor，那么我现在播放 16bit 的 PCM 数据会被 resample 成 32bit 吗？

### 结论

**不会被 resample（重采样），但会被 reformat（格式转换）。**

#### 术语纠正

- **Resample（重采样）**：采样率转换，如 44.1kHz → 48kHz
- **Reformat（格式转换）**：位深度转换，如 16bit → 32bit

#### 实际的数据流

1. **系统启动时**
   - mixPort 配置支持 16bit 和 32bit
   - `pickAudioProfile` 选择 32bit（优先级更高）
   - AudioOutputDescriptor 的 mFormat = 32bit

2. **PlaybackThread 创建**
   - mFormat = 32bit（来自 AudioOutputDescriptor）
   - mMixerBufferFormat = FLOAT（硬编码）

3. **播放 16bit PCM 数据时**
   ```
   16bit PCM 数据
        ↓
   Track 创建 (mFormat = 16bit)
        ↓
   setParameter (MIXER_FORMAT = FLOAT)
        ↓
   ReformatBufferProvider (16bit → FLOAT)
        ↓
   Mixer 混音 (FLOAT 格式)
        ↓
   混音结果 (mMixerBuffer FLOAT)
        ↓
   memcpy_by_audio_format (FLOAT → 32bit)
        ↓
   mSinkBuffer (32bit PCM)
        ↓
   mOutput->write (输出到 HAL)
        ↓
   AudioHAL (32bit PCM)
   ```

#### 关键点

1. **Mixer 内部格式固定为 FLOAT**
   - `mMixerBufferFormat = AUDIO_FORMAT_PCM_FLOAT`（硬编码）
   - 所有 Track 都必须转换为 FLOAT

2. **输入格式转换**
   - 16bit Track → ReformatBufferProvider → FLOAT
   - 这是**精度提升**，不会损失音质

3. **输出格式转换**
   - FLOAT → memcpy_by_audio_format → 32bit
   - 根据 AudioOutputDescriptor 的 mFormat（32bit）转换

4. **最终输出**
   - 输出到 HAL 的是 **32bit PCM**
   - 这是根据 mixPort 配置选择的格式

#### 转换过程总结

| 阶段 | 输入格式 | 输出格式 | 说明 |
|------|---------|---------|------|
| Track 输入 | 16bit PCM | - | 应用提供的数据 |
| Reformat | 16bit | FLOAT | 转换为 Mixer 内部格式 |
| Mixer 混音 | FLOAT | FLOAT | 统一在 FLOAT 格式下混音 |
| 输出转换 | FLOAT | 32bit | 转换为 HAL 输出格式 |
| HAL 输出 | 32bit PCM | - | 输出到硬件 |

#### 关键代码位置

- **Mixer 内部格式设置**：`Threads.cpp:3151` (mMixerBufferFormat = FLOAT)
- **Track MIXER_FORMAT 设置**：`Threads.cpp:6181-6182`
- **输入格式转换**：`AudioMixer.cpp:216-253` (prepareForReformat)
- **输出格式转换**：`Threads.cpp:4477-4478` (memcpy_by_audio_format)

---

## 核心总结

### 格式转换的三个层次

1. **输入层（Track → Mixer）**
   - 所有 Track 的格式（16bit、32bit 等）都转换为 **FLOAT**
   - 通过 ReformatBufferProvider 实现

2. **处理层（Mixer 内部）**
   - 统一使用 **FLOAT** 格式进行混音
   - 高精度，避免精度损失和溢出

3. **输出层（Mixer → HAL）**
   - FLOAT 转换为 **mixPort 配置的格式**（如 32bit）
   - 通过 memcpy_by_audio_format 实现

### 关键设计原则

1. **统一内部格式**：Mixer 内部统一使用 FLOAT，简化混音算法
2. **精度优先**：选择高优先级格式作为输出，避免降质
3. **配置驱动**：输出格式由 mixPort 配置决定，而不是 device
4. **向上转换**：所有 Track 都向上转换到 FLOAT，不会有精度损失

### 验证方法

```bash
# 查看输出流的格式
adb shell dumpsys media.audio_flinger

# 输出示例：
# Output thread 0x12345678:
#   Format: 0x4 (AUDIO_FORMAT_PCM_32_BIT)  ← 输出格式
#   Mixer buffer format: 0x5 (AUDIO_FORMAT_PCM_FLOAT)  ← Mixer 内部格式
```

---

## 相关文件索引

### AudioPolicy 相关
- `audio_policy_configuration.xml` - 音频策略配置文件
- `AudioPolicyManager.cpp` - 音频策略管理器
- `IOProfile.cpp` - mixPort 配置和兼容性检查
- `PolicyAudioPort.cpp` - 格式选择和比较
- `AudioOutputDescriptor.cpp` - 输出描述符

### AudioFlinger 相关
- `Threads.cpp` - PlaybackThread 实现
- `AudioMixer.cpp` - 混音器实现
- `AudioMixerBase.cpp` - 混音器基类

### 音频处理相关
- `libaudioprocessing/` - 音频处理库
- `ReformatBufferProvider` - 格式转换提供者
- `BufferProvider` - 缓冲区提供者链式结构
- 
```
<mixPort name="media_hifi_712" role="source"

                         flags="AUDIO_OUTPUT_FLAG_PRIMARY">

                    <profile name="" format="AUDIO_FORMAT_PCM_32_BIT"

                             samplingRates="48000" channelMasks="AUDIO_CHANNEL_OUT_7POINT1POINT2"/>

                    <profile name="" format="AUDIO_FORMAT_PCM_16_BIT"

                             samplingRates="48000" channelMasks="AUDIO_CHANNEL_OUT_7POINT1POINT2"/>

                    <profile name="" format="AUDIO_FORMAT_PCM_16_BIT"

                             samplingRates="48000" channelMasks="AUDIO_CHANNEL_OUT_5POINT1POINT4"/>

                </mixPort>
```

```
Output thread 0xb4000078077baa50, name AudioOut_25, tid 6637, type 0 (MIXER):

  I/O handle: 37

  Standby: yes

  Sample rate: 48000 Hz

  HAL frame count: 1920

  HAL format: 0x3 (AUDIO_FORMAT_PCM_32_BIT)

  HAL buffer size: 76800 bytes

  Channel count: 10

  Channel mask: 0x000c063f (front-left, front-right, front-center, low-frequency, back-left, back-right, side-left, side-right, top-side-left, top-side-right)

  Processing format: 0x3 (AUDIO_FORMAT_PCM_32_BIT)

  Processing frame size: 40 bytes

  Pending config events: none

  Output devices: 0x1000000 (AUDIO_DEVICE_OUT_BUS)

```

这样就可以看出来MixPort支持三两种FORMAT AUDIO_FORMAT_PCM_32_BIT、AUDIO_FORMAT_PCM_16_BIT ，最后在创建Outout Thread(输出流的时候)format是优先级较高的32bit