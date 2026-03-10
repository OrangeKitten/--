# AAudio 新增 Usage 和 Source 实现计划

  

## 背景

  

需要为 AAudio 添加新的音频 Usage（播放用）和 Source（录音用）：

  

- **播放新增**: `AUDIO_USAGE_SY_ST_ECALL = 103`, `AUDIO_USAGE_SY_ST_CALL = 104`

- **录音新增**: `AUDIO_SOURCE_VOICE_SATCOM_ECALL = 12`, `AUDIO_SOURCE_VOICE_SATCOM_CALL = 13`

  

**重要发现**: 系统 `audio-hal-enums.h` 中已定义这些值：

- `AUDIO_USAGE_SY_ST_ECALL = 103`

- `AUDIO_USAGE_SY_ST_CALL = 104`

- `AUDIO_SOURCE_VOICE_SATCOM_ECALL = 12`

- `AUDIO_SOURCE_VOICE_SATCOM_CALL = 13`

  

AAudio 层只需添加映射，使应用可以使用这些值。

  

---

  

## 实现步骤

  

### 1. 添加 AAudio 公开 API 枚举值

  

**文件**: `/data01/jyxi/8838_new/VENDOR.16.2/frameworks/av/media/libaaudio/include/aaudio/AAudio.h`

  

#### 1.1 添加 Usage 枚举值 (约在第 465 行附近)

在 `AAUDIO_USAGE_ASSISTANT` 之后，`AAUDIO_SYSTEM_USAGE_EMERGENCY` 之前添加：

```cpp

/**

 * ST Emergency Call usage.

 */

AAUDIO_USAGE_SY_ST_ECALL = 103,

  

/**

 * ST Call usage.

 */

AAUDIO_USAGE_SY_ST_CALL = 104,

```

  

#### 1.2 添加 Input Preset 枚举值 (约在第 600 行附近)

在 `aaudio_input_preset_t` 枚举中添加：

```cpp

/**

 * SATCOM Emergency Call input preset.

 */

AAUDIO_INPUT_PRESET_VOICE_SATCOM_ECALL = 12,

  

/**

 * SATCOM Call input preset.

 */

AAUDIO_INPUT_PRESET_VOICE_SATCOM_CALL = 13,

```

  

---

  

### 2. 添加验证逻辑

  

**文件**: `/data01/jyxi/8838_new/VENDOR.16.2/frameworks/av/media/libaaudio/src/core/AAudioStreamParameters.cpp`

  

#### 2.1 更新 Usage 验证 (约第 150-155 行)

在 usage switch 语句中添加新值：

```cpp

case AAUDIO_USAGE_SY_ST_ECALL:

case AAUDIO_USAGE_SY_ST_CALL:

    break; // valid

```

  

#### 2.2 更新 Input Preset 验证 (约第 188-203 行)

在 input preset switch 语句中添加新值：

```cpp

case AAUDIO_INPUT_PRESET_VOICE_SATCOM_ECALL:

case AAUDIO_INPUT_PRESET_VOICE_SATCOM_CALL:

    break; // valid

```

  

---

  

### 3. 添加静态断言验证

  

**文件**: `/data01/jyxi/8838_new/VENDOR.16.2/frameworks/av/media/libaaudio/src/utility/AAudioUtilities.cpp`

  

#### 3.1 更新 Usage 转换函数 (约第 243-269 行)

在 `AAudioConvert_usageToInternal()` 函数中添加静态断言：

```cpp

STATIC_ASSERT(AAUDIO_USAGE_SY_ST_ECALL == AUDIO_USAGE_SY_ST_ECALL);

STATIC_ASSERT(AAUDIO_USAGE_SY_ST_CALL == AUDIO_USAGE_SY_ST_CALL);

```

  

#### 3.2 更新 Input Preset 转换函数 (约第 284-299 行)

在 `AAudioConvert_inputPresetToAudioSource()` 函数中添加静态断言：

```cpp

STATIC_ASSERT(AAUDIO_INPUT_PRESET_VOICE_SATCOM_ECALL == AUDIO_SOURCE_VOICE_SATCOM_ECALL);

STATIC_ASSERT(AAUDIO_INPUT_PRESET_VOICE_SATCOM_CALL == AUDIO_SOURCE_VOICE_SATCOM_CALL);

```

  

---

  

## 涉及文件汇总

  

  

| 文件                                                                  | 修改内容                                                                                                                                                          |
| ------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `frameworks/av/media/libaaudio/include/aaudio/AAudio.h`             | 添加 AAUDIO_USAGE_SY_ST_ECALL (103), AAUDIO_USAGE_SY_ST_CALL (104), AAUDIO_INPUT_PRESET_VOICE_SATCOM_ECALL (12), AAUDIO_INPUT_PRESET_VOICE_SATCOM_CALL (13) 枚举值 |
| `frameworks/av/media/libaaudio/src/core/AAudioStreamParameters.cpp` | validate() 函数中添加新值的 case 分支                                                                                                                                   |
| `frameworks/av/media/libaaudio/src/utility/AAudioUtilities.cpp`     | 转换函数中添加 STATIC_ASSERT 验证                                                                                                                                      |


---

  

## 函数作用说明

  

1. **AAudioStreamParameters::validate()** (`AAudioStreamParameters.cpp`)

   - 作用：验证构建器参数的合法性，确保传入的 usage/inputPreset 是有效值

   - 位置：第 137-160 行 (usage 验证)，第 188-203 行 (input preset 验证)

  

2. **AAudioConvert_usageToInternal()** (`AAudioUtilities.cpp`)

   - 作用：将 AAudio 公开的 usage 转换为 Android 内部 audio_usage_t

   - 使用静态断言确保 AAudio 枚举值与系统枚举值一致

   - 位置：第 243-269 行

  

3. **AAudioConvert_inputPresetToAudioSource()** (`AAudioUtilities.cpp`)

   - 作用：将 AAudio 公开的 input preset 转换为 Android 内部 audio_source_t

   - 使用静态断言确保 AAudio 枚举值与系统枚举值一致

   - 位置：第 284-299 行

  

---

  

## 验证方法

  

1. **编译验证**: 编译 libaaudio 确认无编译错误

2. **静态断言**: 如果枚举值不匹配，编译会报错

3. **单元测试**: 运行 `test_attributes.cpp` 测试用例