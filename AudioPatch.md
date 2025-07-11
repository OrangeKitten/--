# AudioPatch概述
## 定义与作用
- 表示音频端到端连接（Source → Sink）的抽象模型，用于管理音频数据流路径 
- 核心价值：降低延迟（如耳返场景）、支持外部设备直通（如FM收音机）
- Android 5.0+ 引入，提升音频连接管理的抽象层级 
## 核心组件
- Source（源） ：输入设备（MIC）、混音后的音频流 
- Sink（接收器） ：输出设备（扬声器/耳机）、输入混合流 
- AudioPortConfig：描述端口配置（设备类型、地址等）
# 技术实现架构
## API介绍
## 音频通路管理接口
### AudioManager.java
### createAudioPatch()

| 项目 | 说明 |
|------|------|
| `方法签名` | `public static int createAudioPatch(AudioPatch[] patch, AudioPortConfig[] sources, AudioPortConfig[] sinks)` |
| `函数作用` | 创建音频设备之间的连接通路 |
| `参数说明` | `patch` - 用于返回新创建的通路对象<br>`sources` - 源音频端口配置数组<br>`sinks` - 目标音频端口配置数组 |
| `返回值` | `int` - 操作结果(SUCCESS表示成功) |

### releaseAudioPatch()

| 项目 | 说明 |
|------|------|
| `方法签名` | `public static int releaseAudioPatch(AudioPatch patch)` |
| `函数作用` | 释放已存在的音频通路连接 |
| `参数说明` | `patch` - 要释放的音频通路对象 |
| `返回值` | `int` - 操作结果(SUCCESS表示成功) |

### listAudioPatches()

| 项目 | 说明 |
|------|------|
| `方法签名` | `public static int listAudioPatches(ArrayList<AudioPatch> patches)` |
| `函数作用` | 获取所有已存在的音频通路连接列表 |
| `参数说明` | `patches` - 用于返回音频通路列表 |
| `返回值` | `int` - 操作结果(SUCCESS表示成功) |

## 示例代码

```java
// 创建音频通路
AudioPatch[] patch = new AudioPatch[1];
AudioPortConfig[] sources = {sourcePort.buildConfig()};
AudioPortConfig[] sinks = {sinkPort.buildConfig()};
int result = AudioManager.createAudioPatch(patch, sources, sinks);
if (result == AudioManager.SUCCESS) {
    Log.d(TAG, "音频通路创建成功");
}

// 列出所有音频通路
ArrayList<AudioPatch> patches = new ArrayList<>();
result = AudioManager.listAudioPatches(patches);
if (result == AudioManager.SUCCESS) {
    Log.d(TAG, "当前音频通路数量: " + patches.size());
}

// 释放音频通路
result = AudioManager.releaseAudioPatch(patch[0]);
if (result == AudioManager.SUCCESS) {
    Log.d(TAG, "音频通路释放成功");
}
```
### CarAudioManager.java
## 音频通路管理接口

### createGameBoxAudioPatch() 创建游戏投屏通路

| 项目 | 说明 |
|------|------|
| `方法签名` | `public static AudioPatch createGameBoxAudioPatch(AudioPatch audioPatch)` |
| `函数作用` | 创建游戏投屏音频通路 |
| `参数说明` | `audioPatch` - 音频通路对象(可为null) |
| `返回值` | `AudioPatch` - 新创建的音频通路对象 |

### createRadioAudioPatch() 创建收音机通路

| 项目 | 说明 |
|------|------|
| `方法签名` | `public static AudioPatch createRadioAudioPatch(AudioPatch audioPatch)` |
| `函数作用` | 创建收音机音频通路 |
| `参数说明` | `audioPatch` - 音频通路对象(可为null) |
| `返回值` | `AudioPatch` - 新创建的音频通路对象 |

### createBtCallAudioPatch() 创建蓝牙通话通路

| 项目 | 说明 |
|------|------|
| `方法签名` | `public static AudioPatch createBtCallAudioPatch(AudioPatch audioPatch)` |
| `函数作用` | 创建蓝牙通话音频通路 |
| `参数说明` | `audioPatch` - 音频通路对象(可为null) |
| `返回值` | `AudioPatch` - 新创建的音频通路对象 |

### releaseAudioPatch() 释放音频通路

| 项目 | 说明 |
|------|------|
| `方法签名` | `public static int releaseAudioPatch(AudioPatch audioPatch)` |
| `函数作用` | 释放音频通路资源 |
| `参数说明` | `audioPatch` - 要释放的音频通路对象 |
| `返回值` | `int` - 操作结果(0表示成功) |

## 示例代码

```java
// 创建游戏投屏音频通路
AudioPatch gamePatch = CarAudioManager.createGameBoxAudioPatch(null);
Log.d(TAG, "游戏投屏通路创建: " + gamePatch);

// 创建蓝牙通话音频通路 
AudioPatch btPatch = CarAudioManager.createBtCallAudioPatch(null);
Log.d(TAG, "蓝牙通话通路创建: " + btPatch);

// 释放音频通路
int result = CarAudioManager.releaseAudioPatch(gamePatch);
if (result == 0) {
    Log.d(TAG, "游戏投屏通路释放成功");
}
```
## 代码分析

```c++
status_t AudioPolicyManager::createAudioPatchInternal(const struct audio_patch *patch,
                                                      audio_patch_handle_t *handle,
                                                      uid_t uid, uint32_t delayMs,
                                                      const sp<SourceClientDescriptor> &sourceDesc)
{
    // 判断patch的有效性，并且不支持patch有多个souce
    if (!audio_patch_is_valid(patch))
    {
        return BAD_VALUE;
    }
    // only one source per audio patch supported for now
    if (patch->num_sources > 1)
    {
        return INVALID_OPERATION;
    }
    // source必须是数据的输出方
    if (patch->sources[0].role != AUDIO_PORT_ROLE_SOURCE)
    {
        return INVALID_OPERATION;
    }
    // sink必须是数据的输入方
    for (size_t i = 0; i < patch->num_sinks; i++)
    {
        if (patch->sinks[i].role != AUDIO_PORT_ROLE_SINK)
        {
            return INVALID_OPERATION;
        }
    }
    sp<AudioPatch> patchDesc;
    ssize_t index = mAudioPatches.indexOfKey(*handle);
    // 检查当前用户是否有权限修改指定的音频补丁。当尝试修改一个已存在的音频补丁时，系统会验证调用者UID是否与补丁创建者UID匹配。 说白了就是 哪个app创建的patch哪个应用才能修改
    if (index >= 0)
    {
        patchDesc = mAudioPatches.valueAt(index);
        ALOGE("%s mUidCached %d patchDesc->mUid %d uid %d",
              __func__, mUidCached, patchDesc->getUid(), uid);
        if (patchDesc->getUid() != mUidCached && uid != patchDesc->getUid())
        {
            return INVALID_OPERATION;
        }
    }
    else
    {
        *handle = AUDIO_PATCH_HANDLE_NONE;
    }
    // 音频混音端口，通常由 AudioFlinger 提供，用于处理音频流的混音或输出。
    // mix->device
    if (patch->sources[0].type == AUDIO_PORT_TYPE_MIX)
    {
        // 根据id找到对应的SwAudioOutputDescriptor，这个id其实是开机创建的输出流的id
        sp<SwAudioOutputDescriptor> outputDesc = mOutputs.getOutputFromId(patch->sources[0].id);
        ······ DeviceVector devices;
        // 遍历sink，获取对应的DeviceDescriptor，然后与音频参数做匹配，然后合适就加入到devices容器中
        for (size_t i = 0; i < patch->num_sinks; i++)
        {
            // sink必须是device
            if (patch->sinks[i].type != AUDIO_PORT_TYPE_DEVICE)
            {
                ALOGV("%s source mix but sink is not a device", __func__);
                return INVALID_OPERATION;
            }
            // 检查设备支持当前音频参数
            sp<DeviceDescriptor> devDesc =
                mAvailableOutputDevices.getDeviceFromId(patch->sinks[i].id);
            if (!outputDesc->mProfile->isCompatibleProfile(DeviceVector(devDesc),
                                                           patch->sources[0].sample_rate,
                                                           NULL, // updatedSamplingRate
                                                           patch->sources[0].format,
                                                           NULL, // updatedFormat
                                                           patch->sources[0].channel_mask,
                                                           NULL, // updatedChannelMask
                                                           AUDIO_OUTPUT_FLAG_NONE /*FIXME*/))
            {
                ALOGV("%s profile not supported for device %08x", __func__, devDesc->type());
                return INVALID_OPERATION;
            }
            devices.add(devDesc);
        }
        if (devices.size() == 0)
        {
            return INVALID_OPERATION;
        }
        // 将音频输出描述符(outputDesc)的路由切换到指定的设备集合(devices)，并管理相关状态迁移 这个函数很重要接下来在分析
        setOutputDevices(outputDesc, devices, true, 0, handle);
        // 如果没有建立 合适的通路 index 会小于0
        if (index >= 0)
        {
            if (patchDesc != 0 && patchDesc != mAudioPatches.valueAt(index))
            {
                ALOGW("%s setOutputDevice() did not reuse the patch provided", __func__);
            }
            patchDesc = mAudioPatches.valueAt(index);
            patchDesc->setUid(uid);
            ALOGV("%s success", __func__);
        }
        else
        {
            ALOGW("%s setOutputDevice() failed to create a patch", __func__);
            return INVALID_OPERATION;
        }
    }
    else if (patch->sources[0].type == AUDIO_PORT_TYPE_DEVICE)
    {
        // device -> mix
        if (patch->sinks[0].type == AUDIO_PORT_TYPE_MIX)
        {
            // source 为device时，sink为mix，规定sink数量只能有一个
            if (patch->num_sinks > 1)
            {
                return INVALID_OPERATION;
            }
            sp<AudioInputDescriptor> inputDesc = mInputs.getInputFromId(patch->sinks[0].id);
            sp<DeviceDescriptor> device =
                mAvailableInputDevices.getDeviceFromId(patch->sources[0].id);
            // 查看device是否兼容输入流
            if (!inputDesc->mProfile->isCompatibleProfile(DeviceVector(device),
                                                          patch->sinks[0].sample_rate,
                                                          NULL, /*updatedSampleRate*/
                                                          patch->sinks[0].format,
                                                          NULL, /*updatedFormat*/
                                                          patch->sinks[0].channel_mask,
                                                          NULL, /*updatedChannelMask*/
                                                          // FIXME for the parameter type,
                                                          // and the NONE
                                                          (audio_output_flags_t)
                                                              AUDIO_INPUT_FLAG_NONE))
            {
                return INVALID_OPERATION;
            }
            setInputDevice(inputDesc->mIoHandle, device, true, handle);
            if (index >= 0)
            {
                inc__);
                }f (patchDesc != 0 && patchDesc != mAudioPatches.valueAt(index))
                {
                    ALOGW("%s setInputDevice() did not reuse the patch provided", __fu
                patchDesc = mAudioPatches.valueAt(index);
                patchDesc->setUid(uid);
                ALOGV("%s success", __func__);
            }
            else
            {
                ALOGW("%s setInputDevice() failed to create a patch", __func__);
                return INVALID_OPERATION;
            }
        }
        // device -> device
        else if (patch->sinks[0].type == AUDIO_PORT_TYPE_DEVICE)
        {
            sp<DeviceDescriptor> srcDevice =
                mAvailableInputDevices.getDeviceFromId(patch->sources[0].id);
            if (srcDevice == 0)
            {
                return BAD_VALUE;
            }
            PatchBuilder patchBuilder;
            audio_port_config sourcePortConfig = {}
            // toAudioPortConfig 主要是dstConfig->ext.device.hw_module = getModuleHandle();
              srcDevice->toAudioPortConfig(&sourcePortConfig, &patch->sources[0]);
            patchBuilder.addSource(sourcePortConfig);

            for (size_t i = 0; i < patch->num_sinks; i++)
            {
                if (patch->sinks[i].type != AUDIO_PORT_TYPE_DEVICE)
                {
                    ALOGV("%s source device but one sink is not a device", __func__);
                    return INVALID_OPERATION;
                }
                sp<DeviceDescriptor> sinkDevice =
                    mAvailableOutputDevices.getDeviceFromId(patch->sinks[i].id);
                if (sinkDevice == 0)
                {
                    return BAD_VALUE;
                }
                audio_port_config sinkPortConfig = {};
                sinkDevice->toAudioPortConfig(&sinkPortConfig, &patch->sinks[i]);
                patchBuilder.addSink(sinkPortConfig);
                if (!srcDevice->hasSameHwModuleAs(sinkDevice) || // 设备在不同硬件模块
                    (srcDevice->getModuleVersionMajor() < 3) || // HAL版本<3.0
                    !srcDevice->getModule()->supportsPatch(srcDevice, sinkDevice) || // 设备间无硬件直连路由
                    (sourceDesc != nullptr && srcDevice->getAudioPort()->getGains().size() == 0)) // 源设备无增益控制
                {
                    if (patch->num_sinks > 1)
                    {
                        return INVALID_OPERATION;
                    }
                    audio_io_handle_t output = AUDIO_IO_HANDLE_NONE;
                    if (sourceDesc != nullptr) {
                        //这种情况 先不分析
                    }else {
                        //通过sinkDevice选择合适的输出流集合，然后找到最合适的
                        SortedVector<audio_io_handle_t> outputs =
                        getOutputsForDevices(DeviceVector(sinkDevice), mOutputs);
                        // if the sink device is reachable via an opened output stream, request to
                        // go via this output stream by adding a second source to the patch
                        // description
                        output = selectOutput(outputs);
                    }
                        if (output != AUDIO_IO_HANDLE_NONE) {
                        sp<SwAudioOutputDescriptor> outputDesc = mOutputs.valueFor(output);
                        if (outputDesc->isDuplicated()) {
                            ALOGV("%s output for device %s is duplicated",
                                  __FUNCTION__, sinkDevice->toString().c_str());
                            return INVALID_OPERATION;
                        }
                        audio_port_config srcMixPortConfig = {};
                        outputDesc->toAudioPortConfig(&srcMixPortConfig, &patch->sources[0]);
                        if (sourceDesc != nullptr) {
                            sourceDesc->setSwOutput(outputDesc);
                        }
                        // for volume control, we may need a valid stream
                        srcMixPortConfig.ext.mix.usecase.stream = sourceDesc != nullptr ?
                                    sourceDesc->stream() : AUDIO_STREAM_PATCH;
                        //猜测因为两个device之间没有硬件链接，因此需要把数据送入到混合器中，使用混合器做一个桥梁
                        patchBuilder.addSource(srcMixPortConfig);
                    }
                }
            }
                        status_t status = installPatch(
                        __func__, index, handle, patchBuilder.patch(), delayMs, uid, &patchDesc);
        }
        else
        {
            return BAD_VALUE;
        }
    }
    else
    {
        return BAD_VALUE;
    }
```

```Mermaid 
sequenceDiagram
    participant App
    participant AudioManager
    participant AudioPolicyManager

    App->>AudioManager: createAudioPatch()
    AudioManager->>AudioPolicyManager: createAudioPatchInternal()
    
    alt source.type = mix → sink.type = device
        AudioPolicyManager->>AudioPolicyManager: 验证输出设备兼容性
        AudioPolicyManager->>AudioPolicyManager: setOutputDevices()
    else source.type = device
        AudioPolicyManager->>AudioPolicyManager: 检查硬件支持
        alt sink.type = mix  录音情况
        AudioPolicyManager->>AudioPolicyManager: setInputDevice()
        else sink.type = device  无线广播情况
            alt 需要软件桥接
                AudioPolicyManager->>AudioPolicyManager: installPatch(带混音节点)
            else 硬件直连
                AudioPolicyManager->>AudioPolicyManager: installPatch(不带混音节点)
            end
        end
    end
    
    AudioPolicyManager-->>AudioManager: 返回通路句柄
    AudioManager-->>App: 操作结果

```
## 总结
根据audio_patch->source.type 与audio_patch->sink,type来分为三种情况 分别为
1. source.type = AUDIO_PORT_TYPE_MIX与sink.type = AUDIO_PORT_TYPE_DEVICE
   调用setOutputDevices创建链接
2. source.type = AUDIO_PORT_TYPE_DEVICE 与sink.type = AUDIO_PORT_TYPE_MIX
    调用setInputDevices创建链接
3. source.type = AUDIO_PORT_TYPE_DEVICE 与sink.type = AUDIO_PORT_TYPE_DEVICE
   这种情况还要看是否需要创建软件链接如果需要那么
   outputDesc->toAudioPortConfig(&srcMixPortConfig, &patch->sources[0]);
   patchBuilder.addSource(srcMixPortConfig);
   在调用installPatch
```c++
AudioPolicyManager.cpp
status_t AudioPolicyManager::installPatch(const char *caller,
                                          ssize_t index,
                                          audio_patch_handle_t *patchHandle,
                                          const struct audio_patch *patch,
                                          int delayMs,
                                          uid_t uid,
                                          sp<AudioPatch> *patchDescPtr)
{
    sp<AudioPatch> patchDesc;
    audio_patch_handle_t afPatchHandle = AUDIO_PATCH_HANDLE_NONE;
    if (index >= 0) {
        patchDesc = mAudioPatches.valueAt(index);
        afPatchHandle = patchDesc->getAfHandle();
    }

    status_t status = mpClientInterface->createAudioPatch(patch, &afPatchHandle, delayMs);
    if (status == NO_ERROR) {
    if (index < 0) {
            patchDesc = new AudioPatch(patch, uid);
            addAudioPatch(patchDesc->getHandle(), patchDesc);
        } else {
            patchDesc->mPatch = *patch;
        }
        patchDesc->setAfHandle(afPatchHandle);
    if (patchHandle) {
            *patchHandle = patchDesc->getHandle();
        }
        nextAudioPortGeneration();
        mpClientInterface->onAudioPatchListUpdate();
    }
    if (patchDescPtr) *patchDescPtr = patchDesc;
}
```
```c++
PatchPanel.cpp


status_t
AudioFlinger::PatchPanel::createAudioPatch(const struct audio_patch *patch,
                                           audio_patch_handle_t *handle) {
  audio_patch_handle_t halHandle = AUDIO_PATCH_HANDLE_NONE;
  // 检查patch有效性  可以看出来  num_sinks可以是0
  if (!audio_patch_is_valid(patch) ||
      (patch->num_sinks == 0 && patch->num_sources != 2)) {
    return BAD_VALUE;
  }
  if (patch->num_sources > 2) {
    return INVALID_OPERATION;
  }
  // 说白了就是找到经过条件找到oldpatch对应的hwDevice
  // 然后调用releaseAudioPatch，并且erasePatch当前的handle
  if (*handle != AUDIO_PATCH_HANDLE_NONE) {
    auto iter = mPatches.find(*handle);
    if (iter != mPatches.end()) {
      Patch &removedPatch = iter->second;
      if (removedPatch.isSoftware()) {
        // 方法释放播放和捕获线程、音轨以及相关的音频硬件抽象层（HAL）patch
        removedPatch.clearConnections(this);
      }
      // 经过对oldPatch source和sink的判断，与oldpatch与patch的对比
      // 获取到oldpatch对应的 hwModule
      if (hwDevice != 0) {
        hwDevice->releaseAudioPatch(removedPatch.mHalHandle);
      }
      halHandle = removedPatch.mHalHandle;
      erasePatch(*handle);
    }
    Patch newPatch{*patch};
    audio_module_handle_t insertedModule = AUDIO_MODULE_HANDLE_NONE;
    switch (patch->sources[0].type) {
    case AUDIO_PORT_TYPE_DEVICE: {
      audio_module_handle_t srcModule = patch->sources[0].ext.device.hw_module;
      AudioHwDevice *audioHwDevice = findAudioHwDeviceByModule(srcModule);

      for (unsigned int i = 0; i < patch->num_sinks; i++) {
        // 混合器连接限制：当sink类型是AUDIO_PORT_TYPE_MIX（混音器）时，禁止配置多个sink
        // 跨硬件模块限制：当sink设备与源设备不在同一硬件模块时（hw_module !=
        // srcModule），同样禁止多sink配置
        status = INVALID_OPERATION;
        // 混合器连接限制：当sink类型是AUDIO_PORT_TYPE_MIX（混音器）时，禁止配置多个sink
        status = BAD_VALUE;
        goto exit;
      }
      // 满足条件需要创建软件链接
      if ((patch->num_sources == 2) || // 条件1：特殊双源请求
          ((patch->sinks[0].type ==
            AUDIO_PORT_TYPE_DEVICE) && // 条件2：设备到设备路由
           ((patch->sinks[0].ext.device.hw_module !=
             srcModule) || // 子条件2.1：跨硬件模块
            !audioHwDevice
                 ->supportsAudioPatches()))) // 子条件2.2：HAL不支持硬件补丁
      {
        audio_devices_t outputDevice = patch->sinks[0].ext.device.type;
        String8 outputDeviceAddress =
            String8(patch->sinks[0].ext.device.address);
        // 说明当前patch.source是存在一个适配的输出流的
        if (patch->num_sources == 2) {
          if (patch->sources[1].type != AUDIO_PORT_TYPE_MIX ||
              (patch->num_sinks != 0 &&
               patch->sinks[0].ext.device.hw_module !=
                   patch->sources[1].ext.mix.hw_module)) {
            ALOGW("%s() invalid source combination", __func__);
            status = INVALID_OPERATION;
            goto exit;
          }

          sp<ThreadBase> thread = mAudioFlinger.checkPlaybackThread_l(
              patch->sources[1].ext.mix.handle);
          if (thread == 0) {
            ALOGW("%s() cannot get playback thread", __func__);
            status = INVALID_OPERATION;
            goto exit;
          }
          // existing playback thread is reused, so it is not closed when patch
          // is cleared
          newPatch.mPlayback.setThread(
              reinterpret_cast<PlaybackThread *>(thread.get()),
              false /*closeThread*/);
        } else {
          // 根据sink 打开一个新的输出流
        }
        audio_devices_t device = patch->sources[0].ext.device.type;
        String8 address = String8(patch->sources[0].ext.device.address);
        audio_config_t config = AUDIO_CONFIG_INITIALIZER;
        ......
            // 优先使用patch->source的配置 如sample_rate channel_mask
            // 来配置config
            创建录制流 audio_io_handle_t input = AUDIO_IO_HANDLE_NONE;
        sp<ThreadBase> thread = mAudioFlinger.openInput_l(
            srcModule, &input, &config, device, address, AUDIO_SOURCE_MIC,
            flags, outputDevice, outputDeviceAddress);
        newPatch.mRecord.setThread(
            reinterpret_cast<RecordThread *>(thread.get()));
        // 这个函数就比较牛逼了做了一下几件事
        //  创建了一个 source device与 recored thread input的patch
        //   创建了一个playback thread output与 sink device的patch
        //  创建了一个record track 疯狂冲 record thread 中拿数据
        //  创建一个 playback track 给playbackT   hread
        //  送数据，这个track与record track 相同的buffer
        // 这样就打通了一个软件链接
        status = newPatch.createConnections(this);
        if (status != NO_ERROR) {
          goto exit;
        }
        if (audioHwDevice->isInsert()) {
          insertedModule = audioHwDevice->handle();
        }
      } else {
        if (patch->sinks[0].type == AUDIO_PORT_TYPE_MIX) {
          // 查找对应的录制线程
          sp<ThreadBase> thread =
              mAudioFlinger.checkRecordThread_l(patch->sinks[0].ext.mix.handle);
          if (thread == 0) {
            // 回退检查MMAP线程
            thread = mAudioFlinger.checkMmapThread_l(...);
            if (thread == 0) {
              status = BAD_VALUE; // 无效的I/O句柄
              goto exit;
            }
          }

          // 发送补丁配置事件到音频线程
          status = thread->sendCreateAudioPatchConfigEvent(patch, &halHandle);
          if (status == NO_ERROR) {
            newPatch.setThread(thread); // 绑定线程到新补丁
          }

          // 清理同输入端的旧补丁
          for (auto &iter : mPatches) {
            if (iter.second.mAudioPatch.sinks[0].ext.mix.handle ==
                thread->id()) {
              erasePatch(iter.first); // 移除冲突补丁
              break;
            }
          }
        } else {
          sp<DeviceHalInterface> hwDevice = audioHwDevice->hwDevice();
          // 直接调用HAL层创建硬件音频补丁
          status = hwDevice->createAudioPatch(patch->num_sources,
                                              patch->sources, patch->num_sinks,
                                              patch->sinks, &halHandle);

          if (status == INVALID_OPERATION)
            goto exit;
        }
      }
    } break;

    case AUDIO_PORT_TYPE_MIX: {
        audio_module_handle_t srcModule =  patch->sources[0].ext.mix.hw_module;
        DeviceDescriptorBaseVector devices;
        //做了两个限制
       // 1. 如果source类型是mix那么sink类型必须是device
       //2 . 并且source与sink 必须是同一个hw_module
        for (unsigned int i = 0; i < patch->num_sinks; i++) {
                if (patch->sinks[i].type != AUDIO_PORT_TYPE_DEVICE) {
                    ALOGW("%s() invalid sink type %d for mix source",
                            __func__, patch->sinks[i].type);
                    status = BAD_VALUE;
                    goto exit;
                }
                // limit to connections between sinks and sources on same HW module
                if (patch->sinks[i].ext.device.hw_module != srcModule) {
                    status = BAD_VALUE;
                    goto exit;
                }
                sp<DeviceDescriptorBase> device = new DeviceDescriptorBase(
                        patch->sinks[i].ext.device.type);
                device->setAddress(patch->sinks[i].ext.device.address);
                device->applyAudioPortConfig(&patch->sinks[i]);
                devices.push_back(device);
            }
                        sp<ThreadBase> thread =
                            mAudioFlinger.checkPlaybackThread_l(patch->sources[0].ext.mix.handle);
            if (thread == 0) {
                thread = mAudioFlinger.checkMmapThread_l(patch->sources[0].ext.mix.handle);
                if (thread == 0) {
                    ALOGW("%s() bad playback I/O handle %d",
                            __func__, patch->sources[0].ext.mix.handle);
                    status = BAD_VALUE;
                    goto exit;
                }
            }
            if (thread == mAudioFlinger.primaryPlaybackThread_l()) {
                mAudioFlinger.updateOutDevicesForRecordThreads_l(devices);
            }
            //关注这个函数sendCreateAudioPatchConfigEvent
            status = thread->sendCreateAudioPatchConfigEvent(patch, &halHandle);
            if (status == NO_ERROR) {
                newPatch.setThread(thread);
            }
                        // remove stale audio patch with same output as source if any
            for (auto& iter : mPatches) {
                if (iter.second.mAudioPatch.sources[0].ext.mix.handle == thread->id()) {
                    erasePatch(iter.first);
                    break;
                }
            }
    }
    }
  }
  exit:
    ALOGV("%s() status %d", __func__, status);
    if (status == NO_ERROR) {
        *handle = (audio_patch_handle_t) mAudioFlinger.nextUniqueId(AUDIO_UNIQUE_ID_USE_PATCH);
        newPatch.mHalHandle = halHandle;
        mAudioFlinger.mDeviceEffectManager.createAudioPatch(*handle, newPatch);
        mPatches.insert(std::make_pair(*handle, std::move(newPatch)));
        if (insertedModule != AUDIO_MODULE_HANDLE_NONE) {
            addSoftwarePatchToInsertedModules(insertedModule, *handle);
        }
    } else {
        newPatch.clearConnections(this);
    }
    return status;
}
```
##总结
PatchPanel::createAudioPatch是通过 source.type sink.tpye 来区分不同情景的
1. source.type = device  sink.type = device
1.1 如果底层不支持硬件连接，那么就建立软件链路（我们仅分析source与sink都是device的情况），这时候会分别分局souce创建输入流根据sink创建输出流 。
1.1.1 函数createConnections会创建souce_device与recordthread的连接，创建sink_device与outputthread的连接.然后在record的thread中创建一个recordtrack，在outthread中创建一个outputtrack,这两个track buffer地址相同。
1.1.2 这样就完成了软件链接。 比如tuner播放
1.2 如果底层支持硬件链接（且source与sink都是device）那么调用直接hwDevice->createAudioPatch，在audiohal中完成硬件链接
2. source.type = device  sink.type = mix
   首先check一下sink对应的RecordThread是否存在，如果存在那么就使用sendCreateAudioPatchConfigEvent 把source与sink连接在一起 怎么连接的 先不分析（可能也分析不明白嘻嘻）
3. source.type = mix sink.type = device  
   首先check一下source对应的playbackthread，如果存在那么就使用sendCreateAudioPatchConfigEvent 把source与sink连接
## 注意不可以有source.tpye = mix sink.type = mix 的情况

```Mermaid 
sequenceDiagram
    participant App
    participant AudioManager
    participant AudioPolicyManager
    participant PatchPanel
    participant AudioHAL

    App->>AudioManager: createAudioPatch()
    AudioManager->>AudioPolicyManager: createAudioPatchInternal()
    
    alt source=DEVICE → sink=DEVICE
        AudioPolicyManager->>PatchPanel: 检查硬件支持
        alt 需要软件桥接
            PatchPanel->>AudioFlinger: openInput_l()/openOutput_l()
            PatchPanel->>PatchPanel: createConnections()
            Note right of PatchPanel: 创建共享buffer的track对
        else 硬件直连
            PatchPanel->>AudioHAL: createAudioPatch()
        end
    else source=DEVICE → sink=MIX
        PatchPanel->>AudioFlinger: checkRecordThread_l()
        PatchPanel->>RecordThread: sendCreateAudioPatchConfigEvent()
    else source=MIX → sink=DEVICE
        PatchPanel->>AudioFlinger: checkPlaybackThread_l()
        PatchPanel->>PlaybackThread: sendCreateAudioPatchConfigEvent()****
    end

    PatchPanel-->>AudioPolicyManager: 返回通路句柄
    AudioPolicyManager-->>AudioManager: 操作结果
    AudioManager-->>App: 返回状态
```
## Thread.cpp createAudioPatch_l
sendCreateAudioPatchConfigEvent 最终会触发调用createAudioPatch_l 这个函数我就不贴出来了，最后会调用audiohal的createAudioPatch。 从我们自研的3555中可以得知 只处理了source=DEVICE → sink=DEVICE这种情况。 
从AOSP代码可知没创建一个输出流都会调用audiohal的createAudioPatch， 从德赛的audiohal可以看出他们对source->mix 到sink->device的patch进行了处理
```
E APM_AudioPolicyManager: setOutputDevices device {type:0x1000000,@:BUS16_MEDIA_HIFI_714_OUT} delayMs 0
E APM_AudioPolicyManager: setOutputDevices() prevDevice {type:0x1000000,@:BUS16_MEDIA_HIFI_714_OUT}
E APM_AudioPolicyManager: setOutputDevices changing device to {type:0x1000000,@:BUS16_MEDIA_HIFI_714_OUT}
D AHAL: AudioDevice: CreateAudioPatch: 1144: enter: num sources 1, num_sinks 1, *handle 5, sources[0].ext.device.address ����, sinks[0].ext.device.address BUS16_MEDIA_HIFI_714_OUT
D AHAL: AudioDevice: CreateAudioPatch: 1160: source role 1, source type 2, sink role 2, sink type 1, sinks[0] handle 16777216, sources[0] handle 45
D AHAL: AudioDevice: CreateAudioPatch: 1188: Playback patch from mix handle 45 to device 1000000
D AHAL: AudioDevice: CreateAudioPatch: 1300: Exit ret: 0
 ```
他们具体的实现方案因为没有代码也猜不出来了。。
