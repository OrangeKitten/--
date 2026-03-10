好的，让我帮你梳理 Android 16 中从 Audio HAL 获取配置并解析的完整流程。

[9 tools called]

好的，现在让我创建一个完整的流程图来帮你理解 Android 16 中从 Audio HAL 获取配置并解析的流程：

## Android 16 Audio Policy 配置解析流程

### 整体架构

```
┌─────────────────────────────────────────────────────────────────┐
│                        Audio HAL (AIDL)                          │
│  提供: media::AudioPolicyConfig (包含 modules, routes, devices)  │
└────────────────────────────┬────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│              AudioPolicyConfig::loadFromApmAidlConfigWithFallback│
│                        (入口函数)                                 │
└────────────────────────────┬────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│              AudioPolicyConfig::loadFromAidl()                   │
│                   (核心解析函数)                                  │
└────────────────────────────┬────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│         aidl2legacy_AudioHwModules_HwModuleCollection()          │
│              (转换所有 HW Modules)                                │
└────────────────────────────┬────────────────────────────────────┘
                             │
                             ▼
        ┌────────────────────┴────────────────────┐
        │                                         │
        ▼                                         ▼
┌──────────────────────┐              ┌──────────────────────┐
│  解析每个 Module      │              │  解析每个 Module      │
│  (Primary, A2DP...)  │              │  (USB, BT...)        │
└──────┬───────────────┘              └──────┬───────────────┘
       │                                     │
       └──────────────┬──────────────────────┘
                      │
                      ▼
        ┌─────────────────────────────────────────┐
        │  aidl2legacy_AudioHwModule_HwModule()   │
        │        (转换单个 Module)                 │
        └─────────────────┬───────────────────────┘
                          │
        ┌─────────────────┼─────────────────┐
        │                 │                 │
        ▼                 ▼                 ▼
   ┌────────┐      ┌──────────┐      ┌─────────┐
   │ Ports  │      │ Devices  │      │ Routes  │
   └────────┘      └──────────┘      └─────────┘
```

### 详细解析流程

#### 第一步：入口函数

```188:196:d:\Android16\audiopolicy\common\managerdefinitions\src\AudioPolicyConfig.cpp
// static
sp<const AudioPolicyConfig> AudioPolicyConfig::loadFromApmAidlConfigWithFallback(
        const media::AudioPolicyConfig& aidl) {
    auto config = sp<AudioPolicyConfig>::make();
    if (status_t status = config->loadFromAidl(aidl); status == NO_ERROR) {
        return config;
    }
    return createDefault();
}
```

**作用**：
- 创建 `AudioPolicyConfig` 对象
- 调用 `loadFromAidl()` 解析 HAL 提供的配置
- 如果失败，返回默认配置

#### 第二步：核心解析函数

```265:289:d:\Android16\audiopolicy\common\managerdefinitions\src\AudioPolicyConfig.cpp
status_t AudioPolicyConfig::loadFromAidl(const media::AudioPolicyConfig& aidl) {
    RETURN_STATUS_IF_ERROR(aidl2legacy_AudioHwModules_HwModuleCollection(aidl.modules,
                    &mHwModules, &mInputDevices, &mOutputDevices, &mDefaultOutputDevice));
    mIsCallScreenModeSupported = std::find(aidl.supportedModes.begin(), aidl.supportedModes.end(),
            media::audio::common::AudioMode::CALL_SCREEN) != aidl.supportedModes.end();
    mSurroundFormats = VALUE_OR_RETURN_STATUS(
            aidl2legacy_SurroundSoundConfig_SurroundFormats(aidl.surroundSoundConfig));
    mSource = kAidlConfigSource;
    if (aidl.engineConfig.capSpecificConfig.has_value()) {
        setEngineLibraryNameSuffix(kCapEngineLibraryNameSuffix);
#ifdef ENABLE_CAP_AIDL_HYBRID_MODE
        // Using AIDL Audio HAL to get policy configuration and relying on vendor xml configuration
        // file for CAP engine.
#ifndef DISABLE_CAP_AIDL
        if (!aidl.engineConfig.capSpecificConfig.value().domains.has_value()) {
#endif
            mSource = kHybridAidlConfigSource;
#ifndef DISABLE_CAP_AIDL
        }
#endif
#endif
    }
    // No need to augmentData() as AIDL HAL must provide correct mic addresses.
    return NO_ERROR;
}
```

**解析内容**：
1. **HW Modules** (最重要)：调用 `aidl2legacy_AudioHwModules_HwModuleCollection()`
2. **Call Screen Mode**：检查是否支持通话屏幕模式
3. **Surround Formats**：解析环绕声格式配置
4. **Engine Config**：配置音频策略引擎

#### 第三步：解析 HW Modules 集合

```147:158:d:\Android16\audiopolicy\common\managerdefinitions\src\AudioPolicyConfig.cpp
status_t aidl2legacy_AudioHwModules_HwModuleCollection(
        const std::vector<media::AudioHwModule>& aidl,
        HwModuleCollection* legacyModules, DeviceVector* attachedInputDevices,
        DeviceVector* attachedOutputDevices, sp<DeviceDescriptor>* defaultOutputDevice) {
    for (const auto& aidlModule : aidl) {
        sp<HwModule> legacy;
        RETURN_STATUS_IF_ERROR(aidl2legacy_AudioHwModule_HwModule(aidlModule, &legacy,
                        attachedInputDevices, attachedOutputDevices, defaultOutputDevice));
        legacyModules->add(legacy);
    }
    return OK;
}
```

**作用**：
- 遍历 HAL 提供的所有 Module（如 primary, a2dp, usb 等）
- 对每个 Module 调用 `aidl2legacy_AudioHwModule_HwModule()` 进行转换

#### 第四步：解析单个 HW Module（核心）

```68:145:d:\Android16\audiopolicy\common\managerdefinitions\src\AudioPolicyConfig.cpp
status_t aidl2legacy_AudioHwModule_HwModule(const media::AudioHwModule& aidl,
        sp<HwModule>* legacy,
        DeviceVector* attachedInputDevices, DeviceVector* attachedOutputDevices,
        sp<DeviceDescriptor>* defaultOutputDevice) {
    *legacy = sp<HwModule>::make(aidl.name.c_str(), AUDIO_DEVICE_API_VERSION_CURRENT);
    audio_module_handle_t legacyHandle = VALUE_OR_RETURN_STATUS(
            aidl2legacy_int32_t_audio_module_handle_t(aidl.handle));
    (*legacy)->setHandle(legacyHandle);
    IOProfileCollection mixPorts;
    DeviceVector devicePorts;
    const int defaultDeviceFlag = 1 << AudioPortDeviceExt::FLAG_INDEX_DEFAULT_DEVICE;
    std::unordered_map<int32_t, sp<PolicyAudioPort>> ports;
    for (const auto& aidlPort : aidl.ports) {
        const bool isInput = aidlPort.flags.getTag() == AudioIoFlags::input;
        audio_port_v7 legacyPort = VALUE_OR_RETURN_STATUS(
                aidl2legacy_AudioPort_audio_port_v7(aidlPort, isInput));
        // This conversion fills out both 'hal' and 'sys' parts.
        media::AudioPortFw fwPort = VALUE_OR_RETURN_STATUS(
                legacy2aidl_audio_port_v7_AudioPortFw(legacyPort));
        // Since audio_port_v7 lacks some fields, for example, 'maxOpen/ActiveCount',
        // replace the converted data with the actual data from the HAL.
        fwPort.hal = aidlPort;
        if (aidlPort.ext.getTag() == AudioPortExt::mix) {
            auto mixPort = sp<IOProfile>::make("", AUDIO_PORT_ROLE_NONE);
            RETURN_STATUS_IF_ERROR(mixPort->readFromParcelable(fwPort));
            auto& profiles = mixPort->getAudioProfiles();
            if (profiles.empty()) {
                profiles.add(AudioProfile::createFullDynamic(gDynamicFormat));
            } else {
                sortAudioProfiles(mixPort->getAudioProfiles());
            }
            mixPorts.add(mixPort);
            ports.emplace(aidlPort.id, mixPort);
        } else if (aidlPort.ext.getTag() == AudioPortExt::device) {
            // In the legacy XML, device ports use 'tagName' instead of 'AudioPort.name'.
            auto devicePort =
                    sp<DeviceDescriptor>::make(AUDIO_DEVICE_NONE, aidlPort.name);
            RETURN_STATUS_IF_ERROR(devicePort->readFromParcelable(fwPort));
            devicePort->setName("");
            auto& profiles = devicePort->getAudioProfiles();
            if (profiles.empty()) {
                profiles.add(AudioProfile::createFullDynamic(gDynamicFormat));
            } else {
                sortAudioProfiles(profiles);
            }
            devicePorts.add(devicePort);
            ports.emplace(aidlPort.id, devicePort);

            if (const auto& deviceExt = aidlPort.ext.get<AudioPortExt::device>();
                    deviceExt.device.type.connection.empty() ||
                    // DeviceHalAidl connects remote submix input with an address.
                    (deviceExt.device.type.type == AudioDeviceType::IN_SUBMIX &&
                            deviceExt.device.address != AudioDeviceAddress())) {
                // Attached device.
                if (isInput) {
                    attachedInputDevices->add(devicePort);
                } else {
                    attachedOutputDevices->add(devicePort);
                    if (*defaultOutputDevice == nullptr &&
                            (deviceExt.flags & defaultDeviceFlag) != 0) {
                        *defaultOutputDevice = devicePort;
                    }
                }
            }
        } else {
            return BAD_VALUE;
        }
    }
    (*legacy)->setProfiles(mixPorts);
    (*legacy)->setDeclaredDevices(devicePorts);
```

**解析步骤**：

##### 4.1 创建 HwModule 对象
```cpp
*legacy = sp<HwModule>::make(aidl.name.c_str(), AUDIO_DEVICE_API_VERSION_CURRENT);
```

##### 4.2 遍历所有 Ports（关键）
```cpp
std::unordered_map<int32_t, sp<PolicyAudioPort>> ports;  // Port ID 到 Port 对象的映射
for (const auto& aidlPort : aidl.ports) {
```

**Port 分为两类**：

**A. Mix Port（mixport）**：
- 音频流的混音端口（如 `mixport_bus100_bt_a2dp_out`）
- 创建 `IOProfile` 对象
- 添加到 `mixPorts` 集合
- 存入 `ports` 映射表（**Port ID → IOProfile**）

**B. Device Port（devicePort）**：
- 物理设备端口（如 `bus100_bt_a2dp_out`）
- 创建 `DeviceDescriptor` 对象
- 添加到 `devicePorts` 集合
- 存入 `ports` 映射表（**Port ID → DeviceDescriptor**）
- 判断是否为 attached device（永久连接的设备）

##### 4.3 设置 Module 的 Ports 和 Devices
```cpp
(*legacy)->setProfiles(mixPorts);
(*legacy)->setDeclaredDevices(devicePorts);
```

#### 第五步：解析 Routes（路由）

```135:145:d:\Android16\audiopolicy\common\managerdefinitions\src\AudioPolicyConfig.cpp
    AudioRouteVector routes;
    for (const auto& aidlRoute : aidl.routes) {
        sp<AudioRoute> legacy = VALUE_OR_RETURN_STATUS(aidl2legacy_AudioRoute(aidlRoute, ports));
        routes.add(legacy);
    }
    (*legacy)->setRoutes(routes);
    return OK;
}
```

**关键函数**：`aidl2legacy_AudioRoute()`

```48:66:d:\Android16\audiopolicy\common\managerdefinitions\src\AudioPolicyConfig.cpp
ConversionResult<sp<AudioRoute>>
aidl2legacy_AudioRoute(const media::AudioRoute& aidl,
        const std::unordered_map<int32_t, sp<PolicyAudioPort>>& ports) {
    auto legacy = sp<AudioRoute>::make(aidl.isExclusive ? AUDIO_ROUTE_MUX : AUDIO_ROUTE_MIX);
    auto legacySink = VALUE_OR_RETURN(aidl2legacy_portId_PolicyAudioPort(aidl.sinkPortId, ports));
    legacy->setSink(legacySink);
    PolicyAudioPortVector legacySources;
    for (int32_t portId : aidl.sourcePortIds) {
        sp<PolicyAudioPort> legacyPort = VALUE_OR_RETURN(
                aidl2legacy_portId_PolicyAudioPort(portId, ports));
        legacySources.add(legacyPort);
    }
    legacy->setSources(legacySources);
    legacySink->addRoute(legacy);
    for (const auto& legacySource : legacySources) {
        legacySource->addRoute(legacy);
    }
    return legacy;
}
```

**解析步骤**：

1. **创建 AudioRoute 对象**：
   - `isExclusive = true` → `AUDIO_ROUTE_MUX`（互斥路由）
   - `isExclusive = false` → `AUDIO_ROUTE_MIX`（混音路由）

2. **通过 Port ID 查找 Sink**：
   ```cpp
   auto legacySink = VALUE_OR_RETURN(aidl2legacy_portId_PolicyAudioPort(aidl.sinkPortId, ports));
   ```
   - 从 `ports` 映射表中查找 `sinkPortId`
   - **这就是你的问题所在！如果 HAL 提供的 sinkPortId 在 ports 映射表中找不到，就会失败**

3. **通过 Port ID 查找 Sources**：
   ```cpp
   for (int32_t portId : aidl.sourcePortIds) {
       sp<PolicyAudioPort> legacyPort = VALUE_OR_RETURN(
               aidl2legacy_portId_PolicyAudioPort(portId, ports));
       legacySources.add(legacyPort);
   }
   ```

4. **建立双向关联**：
   ```cpp
   legacySink->addRoute(legacy);      // Sink 添加路由
   for (const auto& legacySource : legacySources) {
       legacySource->addRoute(legacy);  // 每个 Source 添加路由
   }
   ```

### 你的问题根源

根据你的描述：
```
- Audio Routes (5):
      5. Mix; Sink: "bus100_bt_a2dp_out"
         Sources: "mixport_bus100_bt_a2dp_out"
   3. "mixport_bus100_bt_a2dp_out"; 0x0000 (AUDIO_OUTPUT_FLAG_NONE)
但是device没有bus100_bt_a2dp_out
```

**问题分析**：

1. **HAL 提供的 Route 配置**：
   - Sink Port ID → 指向名为 `bus100_bt_a2dp_out` 的设备
   - Source Port ID → 指向名为 `mixport_bus100_bt_a2dp_out` 的 mixport

2. **HAL 提供的 Ports 配置**：
   - ✅ 有 `mixport_bus100_bt_a2dp_out`（Mix Port）
   - ❌ **没有** `bus100_bt_a2dp_out`（Device Port）

3. **解析失败原因**：
   ```cpp
   // 在 aidl2legacy_AudioRoute() 中
   auto legacySink = VALUE_OR_RETURN(aidl2legacy_portId_PolicyAudioPort(aidl.sinkPortId, ports));
   ```
   - 查找 `sinkPortId` 时，在 `ports` 映射表中找不到对应的 Device Port
   - 返回 `BAD_VALUE` 错误

### 解决方案

**方案 1：要求 HAL 提供完整的 Device Port**

Audio HAL 需要在 `media::AudioHwModule.ports` 中添加 `bus100_bt_a2dp_out` 的 Device Port 定义：

```cpp
// HAL 应该提供类似这样的配置
media::AudioPort devicePort;
devicePort.id = <unique_port_id>;  // 唯一的 Port ID
devicePort.name = "bus100_bt_a2dp_out";
devicePort.ext = AudioPortExt::device(...);  // Device Port 扩展信息
// ... 其他配置
```

**方案 2：检查 HAL 的 Route 配置**

确认 HAL 提供的 `media::AudioRoute` 中的 `sinkPortId` 是否正确：

```cpp
media::AudioRoute route;
route.sinkPortId = <device_port_id>;  // 必须对应一个已定义的 Device Port
route.sourcePortIds = {<mixport_id>};  // 对应 mixport_bus100_bt_a2dp_out
```

### 调试建议

在 `aidl2legacy_AudioHwModule_HwModule()` 函数中添加日志：

```cpp
// 在解析完所有 ports 后
ALOGD("=== Parsed Ports ===");
for (const auto& [portId, port] : ports) {
    ALOGD("Port ID: %d, Name: %s", portId, port->getTagName().c_str());
}

// 在解析 routes 前
ALOGD("=== Parsing Routes ===");
for (const auto& aidlRoute : aidl.routes) {
    ALOGD("Route: sinkPortId=%d, sourcePortIds count=%zu", 
          aidlRoute.sinkPortId, aidlRoute.sourcePortIds.size());
}
```

这样可以清楚地看到：
1. HAL 提供了哪些 Ports
2. Route 引用了哪些 Port IDs
3. 哪个 Port ID 找不到

希望这个详细的流程梳理能帮助你理解问题所在！关键是确保 **HAL 提供的每个 Route 的 sinkPortId 和 sourcePortIds 都能在 ports 列表中找到对应的定义**。


让我帮你梳理 `mOutputDevices` 的添加流程：

[1 tool called]

现在让我搜索所有添加到 `mOutputDevices` 的地方：

[4 tools called]

现在让我查看 XML 解析时如何添加设备：

[2 tools called]

完美！现在我可以完整梳理 `mOutputDevices` 的添加流程了。让我创建一个详细的流程图：

## `mOutputDevices` 添加流程完整梳理

### 一、添加时机和方式

`mOutputDevices` 在以下**三种场景**下会被添加设备：

---

### **场景 1：从 AIDL HAL 加载配置**

#### 调用链：
```
AudioPolicyConfig::loadFromAidl()
  ↓
aidl2legacy_AudioHwModules_HwModuleCollection()
  ↓
aidl2legacy_AudioHwModule_HwModule()
  ↓
attachedOutputDevices->add(devicePort)  // 临时变量
  ↓
最终传递给 mOutputDevices
```

#### 详细流程：

```267:267:/data01/jyxi/8838_new/VENDOR.16.2/frameworks/av/services/audiopolicy/common/managerdefinitions/src/AudioPolicyConfig.cpp
RETURN_STATUS_IF_ERROR(aidl2legacy_AudioHwModules_HwModuleCollection(aidl.modules,
                &mHwModules, &mInputDevices, &mOutputDevices, &mDefaultOutputDevice));
```

**步骤：**

1. **遍历 AIDL 模块中的所有端口**
   ```cpp
   for (const auto& aidlPort : aidl.ports)
   ```

2. **判断是否为 device port**
   ```cpp
   if (aidlPort.ext.getTag() == AudioPortExt::device)
   ```

3. **创建 DeviceDescriptor**
   ```cpp
   auto devicePort = sp<DeviceDescriptor>::make(AUDIO_DEVICE_NONE, aidlPort.name);
   devicePort->readFromParcelable(fwPort);
   ```

4. **判断是否为已连接设备**（关键判断）
   ```cpp
   if (deviceExt.device.type.connection.empty() ||
       (deviceExt.device.type.type == AudioDeviceType::IN_SUBMIX &&
        deviceExt.device.address != AudioDeviceAddress()))
   ```

5. **添加到输出设备列表**
   ```cpp
   if (!isInput) {
       attachedOutputDevices->add(devicePort);
   }
   ```

6. **最终赋值给 mOutputDevices**
   - `attachedOutputDevices` 作为引用参数传递
   - 直接指向 `mOutputDevices`

---

### **场景 2：从 XML 文件加载配置**

#### 调用链：
```
AudioPolicyConfig::loadFromXml()
  ↓
deserializeAudioPolicyFile()
  ↓
PolicySerializer::deserialize()
  ↓
deserializeCollection<ModuleTraits>()
  ↓
deserialize<ModuleTraits>()
  ↓
处理 <attachedDevices> 标签
  ↓
ctx->addDevice(device)
  ↓
AudioPolicyConfig::addDevice()
  ↓
mOutputDevices.add(device)
```

#### 详细流程：

**步骤 1：解析 devicePort**

```526:600:/data01/jyxi/8838_new/VENDOR.16.2/frameworks/av/services/audiopolicy/common/managerdefinitions/src/Serializer.cpp
std::variant<status_t, DevicePortTraits::Element> PolicySerializer::deserialize<DevicePortTraits>(
        const xmlNode *cur, DevicePortTraits::PtrSerializingCtx /*serializingContext*/)
{
    // 读取 tagName, type, role, address 等属性
    std::string name = getXmlAttribute(cur, Attributes::tagName);
    std::string typeName = getXmlAttribute(cur, Attributes::type);
    std::string role = getXmlAttribute(cur, Attributes::role);
    std::string address = getXmlAttribute(cur, Attributes::address);
    
    // 创建 DeviceDescriptor
    DevicePortTraits::Element deviceDesc =
            new DeviceDescriptor(type, name, address, encodedFormats);
    
    return deviceDesc;
}
```

**步骤 2：解析 Module 时处理 attachedDevices**

```xml
<module name="default">
    <attachedDevices>
        <item>Media Bus</item>
        <item>Nav Guidance Bus</item>
        ...
    </attachedDevices>
    ...
</module>
```

对应代码：

```820:850:/data01/jyxi/8838_new/VENDOR.16.2/frameworks/av/services/audiopolicy/common/managerdefinitions/src/Serializer.cpp
for (const xmlNode *children = cur->xmlChildrenNode; children != NULL;
     children = children->next) {
    if (!xmlStrcmp(children->name, reinterpret_cast<const xmlChar*>(childAttachedDevicesTag))) {
        ALOGV("%s: %s %s found", __func__, tag, childAttachedDevicesTag);
        for (const xmlNode *child = children->xmlChildrenNode; child != NULL;
             child = child->next) {
            if (!xmlStrcmp(child->name,
                            reinterpret_cast<const xmlChar*>(childAttachedDeviceTag))) {
                auto attachedDevice = make_xmlUnique(xmlNodeListGetString(
                                child->doc, child->xmlChildrenNode, 1));
                if (attachedDevice != nullptr) {
                    ALOGV("%s: %s %s=%s", __func__, tag, childAttachedDeviceTag,
                            reinterpret_cast<const char*>(attachedDevice.get()));
                    sp<DeviceDescriptor> device = module->getDeclaredDevices().
                            getDeviceFromTagName(std::string(reinterpret_cast<const char*>(
                                                    attachedDevice.get())));
                    if (device == NULL) {
                        // 错误处理
                        continue;
                    }
                    ctx->addDevice(device);  // 关键：添加设备
                }
            }
        }
    }
}
```

**步骤 3：addDevice() 方法**

```107:123:/data01/jyxi/8838_new/VENDOR.16.2/frameworks/av/services/audiopolicy/common/managerdefinitions/include/AudioPolicyConfig.h
void addDevice(const sp<DeviceDescriptor> &device)
{
    if (audio_is_output_device(device->type())) {
        mOutputDevices.add(device);
    } else if (audio_is_input_device(device->type())) {
        mInputDevices.add(device);
    }
}
```

---

### **场景 3：使用默认配置（Fallback）**

#### 调用链：
```
AudioPolicyConfig::createDefault()
  ↓
AudioPolicyConfig::setDefault()
  ↓
mOutputDevices.add(mDefaultOutputDevice)
```

#### 详细流程：

```307:319:/data01/jyxi/8838_new/VENDOR.16.2/frameworks/av/services/audiopolicy/common/managerdefinitions/src/AudioPolicyConfig.cpp
void AudioPolicyConfig::setDefault() {
    mSource = kDefaultConfigSource;
    mEngineLibraryNameSuffix = kDefaultEngineLibraryNameSuffix;

    mDefaultOutputDevice = new DeviceDescriptor(AUDIO_DEVICE_OUT_SPEAKER);
    mDefaultOutputDevice->addAudioProfile(AudioProfile::createFullDynamic(gDynamicFormat));
    sp<DeviceDescriptor> defaultInputDevice = new DeviceDescriptor(AUDIO_DEVICE_IN_BUILTIN_MIC);
    defaultInputDevice->addAudioProfile(AudioProfile::createFullDynamic(gDynamicFormat));
    sp<AudioProfile> micProfile = new AudioProfile(
            AUDIO_FORMAT_PCM_16_BIT, AUDIO_CHANNEL_IN_MONO, 8000);
    defaultInputDevice->addAudioProfile(micProfile);
    mOutputDevices.add(mDefaultOutputDevice);  // 添加默认扬声器
    mInputDevices.add(defaultInputDevice);
    // ... 其他初始化
}
```

---

### 二、完整流程图

```
┌─────────────────────────────────────────────────────────────┐
│          AudioPolicyConfig 初始化                            │
└─────────────────────────────────────────────────────────────┘
                            │
                ┌───────────┴───────────┐
                │                       │
        ┌───────▼────────┐      ┌──────▼──────────┐
        │  AIDL HAL 配置  │      │   XML 文件配置   │
        └───────┬────────┘      └──────┬──────────┘
                │                       │
                │                       │
    ┌───────────▼──────────┐   ┌───────▼────────────┐
    │ loadFromAidl()       │   │ loadFromXml()      │
    └───────────┬──────────┘   └───────┬────────────┘
                │                       │
                │                       │
    ┌───────────▼──────────────────────▼────────────┐
    │  aidl2legacy_AudioHwModules_HwModuleCollection│
    │  或 deserializeAudioPolicyFile                │
    └───────────┬───────────────────────────────────┘
                │
                │
    ┌───────────▼──────────────────────────────────┐
    │  遍历所有 Module                              │
    └───────────┬──────────────────────────────────┘
                │
                │
    ┌───────────▼──────────────────────────────────┐
    │  解析 devicePort                              │
    │  - 读取 tagName, type, role, address         │
    │  - 创建 DeviceDescriptor                     │
    └───────────┬──────────────────────────────────┘
                │
                │
    ┌───────────▼──────────────────────────────────┐
    │  判断是否为已连接设备                          │
    │  AIDL: connection.empty() 或特殊 submix      │
    │  XML:  在 <attachedDevices> 中列出           │
    └───────────┬──────────────────────────────────┘
                │
                │ 是已连接设备
                │
    ┌───────────▼──────────────────────────────────┐
    │  判断设备类型                                  │
    └───────────┬──────────────────────────────────┘
                │
        ┌───────┴────────┐
        │                │
    ┌───▼────┐      ┌────▼────┐
    │ 输出设备 │      │ 输入设备 │
    └───┬────┘      └────┬────┘
        │                │
        │                │
    ┌───▼────────────────▼────┐
    │ mOutputDevices.add()    │
    │ mInputDevices.add()     │
    └─────────────────────────┘
```

---

### 三、关键判断条件对比

| 配置方式 | 判断是否为已连接设备的条件 |
|---------|-------------------------|
| **AIDL HAL** | `connection.empty()` 或 `(IN_SUBMIX && 有地址)` |
| **XML 文件** | 在 `<attachedDevices>` 标签中明确列出 |
| **默认配置** | 直接添加 `OUT_SPEAKER` 和 `IN_BUILTIN_MIC` |

---

### 四、以你的 XML 配置为例

```xml
<module name="default">
    <attachedDevices>
        <item>Media Bus</item>
        <item>Nav Guidance Bus</item>
        <item>TTS VR Bus</item>
        <item>Sys Notification Bus</item>
        <item>Auto TTS Bus</item>
        <item>tuner_in</item>
        <item>bus100_bt_a2dp_out</item>
    </attachedDevices>
    
    <devicePorts>
        <devicePort tagName="Media Bus" role="sink" attached="true" 
                    deviceType="OUT_BUS" address="BUS00_MEDIA">
            ...
        </devicePort>
        ...
    </devicePorts>
</module>
```

**执行流程：**

1. **解析 `<devicePorts>`**
   - 创建所有 DeviceDescriptor 对象
   - 存储在 `module->getDeclaredDevices()` 中

2. **解析 `<attachedDevices>`**
   - 读取 `<item>Media Bus</item>`
   - 从 `getDeclaredDevices()` 中查找 tagName="Media Bus" 的设备
   - 调用 `ctx->addDevice(device)`
   - 最终调用 `mOutputDevices.add(device)`

3. **结果**
   - `mOutputDevices` 包含：Media Bus, Nav Guidance Bus, TTS VR Bus, Sys Notification Bus, Auto TTS Bus, bus100_bt_a2dp_out
   - `mInputDevices` 包含：tuner_in

---

### 五、总结

**`mOutputDevices` 的添加时机：**

1. ✅ **系统启动时**：从 AIDL HAL 或 XML 配置文件加载
2. ✅ **Fallback 时**：使用默认配置
3. ❌ **运行时动态添加**：`mOutputDevices` 在 AudioPolicyConfig 中是**只读的**，运行时的设备连接/断开由 AudioPolicyManager 管理

**关键点：**
- `mOutputDevices` 只包含**已连接的输出设备**
- 对于 XML 配置，必须在 `<attachedDevices>` 中明确列出
- 对于 AIDL 配置，根据 `connection` 字段自动判断
- 可插拔设备（蓝牙、USB 等）**不会**在初始化时添加到 `mOutputDevices`