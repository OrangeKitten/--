# audioserver 获取 android.hardware.audio.service 完整流程

让我详细梳理这个双向通信的建立过程，包括**服务端注册**和**客户端连接**。

---

## 🔄 完整流程概览

```
┌──────────────────────────────────────────────────────────────────────┐
│                           系统启动阶段                                  │
└──────────────────────────────────────────────────────────────────────┘
                                │
                ┌───────────────┴───────────────┐
                │                               │
        ┌───────▼────────┐             ┌────────▼───────┐
        │  Service 端     │             │   Client 端     │
        │ (HAL Service)   │             │  (audioserver)  │
        └───────┬────────┘             └────────┬───────┘
                │                               │
        [1] 启动并初始化                  [4] 需要 Audio HAL
                │                               │
                ├─ 启动 AIDL 线程池              ├─ [5] 版本检测
                └─ 启动 HIDL 线程池              │   (按优先级依次检测)
                │                               │
        [2] 动态加载实现库                ├─ 🥇 检测 AIDL 1.0
                │                         │    ↓ AServiceManager_isDeclared()
                ├─ AIDL 实现库              │    ↓ 查询 servicemanager
                │  (可选)                  │    ↓ (/dev/binder)
                └─ HIDL 实现库              │    │
                   (@7.0-impl.so)         │    ├─ 找到? → 使用 AIDL路径
                │                         │    └─ 未找到? ↓
        [3] 注册到对应的                  │
            ServiceManager               ├─ 🥈 检测 HIDL 7.1
                │                         │    ↓ IServiceManager::getTransport()
                ├─ AIDL 服务注册            │    ↓ 查询 hwservicemanager  
                │  → servicemanager        │    ↓ (/dev/hwbinder)
                │    (/dev/binder)         │    │
                │                         │    ├─ 找到? → 使用 HIDL 7.1
                └─ HIDL 服务注册            │    └─ 未找到? ↓
                   → hwservicemanager     │
                     (/dev/hwbinder)      ├─ 🥉 检测 HIDL 7.0
                │                         │    ↓ (同上)
                │                         │    ├─ 找到? → 使用 HIDL 7.0
                ↓ (等待客户端连接)          │    └─ 未找到? ↓
                                          │
                                          └─ ④ 检测 HIDL 6.0
                                               ↓ (同上)
                                               └─ 找到或失败
                                          │
                                  [6] 根据检测结果加载包装库
                                          │
                      ┌───────────────────┴───────────────────┐
                      │                                       │
              选择 AIDL 路径                           选择 HIDL 路径
                      │                                       │
            [7a] libaudiohal@1.0.so               [7b] libaudiohal@7.0.so
                      │                                       │
                      ├─ getServiceInstance()                ├─ IDevicesFactory::getService()
                      ├─ AServiceManager_wait..              ├─ 通过 hwservicemanager
                      └─ 获取 IConfig                        └─ 获取 IDevicesFactory
                      │                                       │
                      └───────────────────┬───────────────────┘
                                          │
                                  [8] 获取服务代理对象
                                          │
                      ┌───────────────────┴───────────────────┐
                      │         IPC 连接建立完成                │
                      │  • AIDL: Binder (/dev/binder)          │
                      │  • HIDL: HwBinder (/dev/hwbinder)      │
                      └───────────────────┬───────────────────┘
                                          │
                                          ▼
                                  ┌───────────────┐
                                  │  通信通道就绪  │
                                  │  客户端可调用  │
                                  └───────────────┘
```

**🎯 关键决策点说明：**

| 决策点 | AIDL路径 | HIDL路径 |
|-------|---------|---------|
| **[5] 检测顺序** | 🥇 优先级最高 | 🥈🥉④ 依次检测 7.1 → 7.0 → 6.0 |
| **检测API** | `AServiceManager_isDeclared()` | `IServiceManager::getTransport()` |
| **ServiceManager** | `servicemanager` | `hwservicemanager` |
| **设备节点** | `/dev/binder` | `/dev/hwbinder` |
| **[6] 包装库** | `libaudiohal@1.0.so` | `libaudiohal@7.x.so` |
| **[7] 获取接口** | `IConfig` | `IDevicesFactory` |
| **服务名格式** | `pkg.interface/instance` | `pkg@version::interface` |

---

## 📍 服务端流程（android.hardware.audio.service）

### Step 1️⃣: 服务启动（Init 进程启动）

```bash
# /vendor/etc/init/android.hardware.audio.service.rc
service vendor.audio-hal /vendor/bin/hw/android.hardware.audio.service
    class hal
    user audioserver
    ...
```

### Step 2️⃣: 主函数初始化 Binder 环境

```cpp
@d:\code\hardware\audio\audio\common\all-versions\default\service\service.cpp#87-108
int main(int /* argc */, char* /* argv */ []) {
    signal(SIGPIPE, SIG_IGN);
    
    // 初始化 HwBinder（用于 HIDL）
    android::hardware::ProcessState::initWithMmapSize(...);
    
    // 初始化 Binder（用于 AIDL）
    if (::android::ProcessState::isVndservicemanagerEnabled()) {
        ::android::ProcessState::initWithDriver("/dev/vndbinder");
        ::android::ProcessState::self()->startThreadPool();
    }
    
    // 启动 AIDL Binder 线程池
    ABinderProcess_setThreadPoolMaxThreadCount(1);
    ABinderProcess_startThreadPool();
    
    // 启动 HIDL Binder 线程池
    configureRpcThreadpool(16, true);
    ...
}
```

### Step 3️⃣: 遍历版本列表，动态加载 HAL 实现

```cpp
@d:\code\hardware\audio\audio\common\all-versions\default\service\service.cpp#112-128
const std::vector<InterfacesList> mandatoryInterfaces = {
    {
        "Audio Core API",
        "android.hardware.audio@7.1::IDevicesFactory",  // 从新到旧
        "android.hardware.audio@7.0::IDevicesFactory",
        "android.hardware.audio@6.0::IDevicesFactory",
        ...
    },
};

// 遍历并注册
for (const auto& listIter : mandatoryInterfaces) {
    registerPassthroughServiceImplementations(iter, listIter.end());
}
```

### Step 4️⃣: Passthrough 模式加载实现库

```cpp
@d:\code\hardware\audio\audio\common\all-versions\default\service\service.cpp#52-60
template <class Iter>
static bool registerPassthroughServiceImplementations(Iter first, Iter last) {
    for (; first != last; ++first) {
        // 尝试加载 android.hardware.audio@7.1-impl.so
        if (registerPassthroughServiceImplementation(*first) == OK) {
            return true;  // 成功就停止
        }
        // 失败则继续尝试 @7.0-impl.so, @6.0-impl.so...
    }
    return false;
}
```

**内部执行：**
1. `dlopen("android.hardware.audio@7.0-impl.so")` 加载库
2. `dlsym()` 获取 `HIDL_FETCH_IDevicesFactory` 函数指针
3. 调用工厂函数创建 `IDevicesFactory` 实例
4. 调用 `registerAsService()` 注册到 **hwservicemanager**

### Step 5️⃣: 可选服务加载（AIDL 方式）

```cpp
@d:\code\hardware\audio\audio\common\all-versions\default\service\service.cpp#62-85
static bool registerExternalServiceImplementation(const std::string& libName,
                                                  const std::string& funcName) {
    // dlopen("android.hardware.bluetooth.audio-impl.so")
    handle = dlopen(libPath.c_str(), dlMode);
    
    // dlsym 获取工厂函数
    binder_status_t (*factoryFunction)();
    *(void**)(&factoryFunction) = dlsym(handle, funcName.c_str());
    
    // 调用工厂函数，内部会注册 AIDL 服务到 servicemanager
    return ((*factoryFunction)() == STATUS_OK);
}
```

**服务端流程完成：** 服务已注册到系统，等待客户端连接。

---

## 📍 客户端流程（audioserver）

### Step 1️⃣: audioserver 启动，需要 Audio HAL

```cpp
// 在 AudioFlinger 或其他组件初始化时
sp<DevicesFactoryHalInterface> factory = DevicesFactoryHalInterface::create();
```

### Step 2️⃣: 服务检测实现（核心）

#### 🎯 统一入口函数

```cpp
@d:\code\Audio相关\framework\libaudiohal\FactoryHal.cpp#137-147
bool hasHalService(const InterfaceName& interface, const AudioHalVersionInfo& version) {
    auto halType = version.getType();
    
    if (halType == AudioHalVersionInfo::Type::AIDL) {
        return hasAidlHalService(interface, version);  // AIDL 检测
    } else if (halType == AudioHalVersionInfo::Type::HIDL) {
        return hasHidlHalService(interface, version);  // HIDL 检测
    } else {
        ALOGE("HalType not supported %s", version.toString().c_str());
        return false;
    }
}
```

#### 📱 AIDL 服务检测详解（Android 13+）

```cpp
@d:\code\Audio相关\framework\libaudiohal\FactoryHal.cpp#103-112
bool hasAidlHalService(const InterfaceName& interface, const AudioHalVersionInfo& version) {
    // 构造完整服务名：package.interface/instance
    // 例如：android.hardware.audio.core.IModule/default
    const std::string name = interface.first + "." + interface.second + "/default";
    
    // 使用 NDK Binder API 检查服务是否在 servicemanager 中声明
    const bool isDeclared = AServiceManager_isDeclared(name.c_str());
    
    if (!isDeclared) {
        ALOGW("%s %s: false", __func__, name.c_str());
        return false;
    }
    
    ALOGI("%s %s: true, version %s", __func__, name.c_str(), 
          version.toString().c_str());
    return true;
}
```

**关键API说明：**

| API | 功能 | 返回值 | 阻塞性 |
|-----|------|--------|-------|
| `AServiceManager_isDeclared()` | 检查服务是否在 manifest 中声明 | bool | ❌ 非阻塞 |
| `AServiceManager_checkService()` | 检查服务是否运行 | AIBinder* | ❌ 非阻塞 |
| `AServiceManager_waitForService()` | 等待服务启动 | AIBinder* | ✅ 阻塞 |

#### 📱 HIDL 服务检测详解（Android 8-12）

```cpp
@d:\code\Audio相关\framework\libaudiohal\FactoryHal.cpp#114-135
bool hasHidlHalService(const InterfaceName& interface, const AudioHalVersionInfo& version) {
    using ::android::hidl::manager::V1_0::IServiceManager;
    
    // 获取 HIDL ServiceManager
    sp<IServiceManager> sm = ::android::hardware::defaultServiceManager();
    if (!sm) {
        ALOGW("Failed to obtain HIDL ServiceManager");
        return false;
    }
    
    // 构造 HIDL 完全限定名（FQN）
    // 例如：android.hardware.audio@7.0::IDevicesFactory
    const std::string fqName = 
        interface.first + "@" + version.toVersionString() + "::" + interface.second;
    const std::string instance = "default";
    
    // 查询传输类型（避免直接实例化服务，因为音频 HAL 不支持多客户端）
    using Transport = IServiceManager::Transport;
    Return<Transport> transport = sm->getTransport(fqName, instance);
    
    if (!transport.isOk()) {
        ALOGW("Failed to obtain transport type for %s/%s: %s",
              fqName.c_str(), instance.c_str(), transport.description().c_str());
        return false;
    }
    
    // Transport::EMPTY 表示服务不存在
    return transport != Transport::EMPTY;
}
```

**HIDL Transport 类型：**

| Transport 类型 | 说明 | 使用场景 |
|---------------|------|---------|
| `HWBINDER` | Binder IPC（跨进程） | 标准HAL服务 |
| `PASSTHROUGH` | 直接调用（同进程） | 老版本兼容 |
| `EMPTY` | 服务不存在 | 查询失败 |

---

### Step 2.6️⃣: AIDL vs HIDL 检测对比

#### 📊 对比表

| 特性 | AIDL | HIDL |
|-----|------|------|
| **ServiceManager** | `servicemanager`（/dev/binder） | `hwservicemanager`（/dev/hwbinder） |
| **服务名格式** | `package.interface/instance`<br>例：`android.hardware.audio.core.IModule/default` | `package@version::interface`<br>例：`android.hardware.audio@7.0::IDevicesFactory` |
| **检测API** | `AServiceManager_isDeclared()` | `IServiceManager::getTransport()` |
| **头文件** | `<android/binder_manager.h>` | `<hidl/ServiceManagement.h>` |
| **库依赖** | `libbinder_ndk` | `libhidlbase` |
| **实例名称** | 包含在服务名中（/default） | 单独参数（"default"） |
| **版本表示** | 不在服务名中 | 在FQN中（@7.0） |
| **多实例支持** | ✅ 原生支持 | ⚠️ 需手动处理 |

#### 🔄 服务查询流程对比

```
┌─────────────────────────────────────────────────────────────┐
│                      AIDL 服务检测流程                         │
└─────────────────────────────────────────────────────────────┘
audioserver
    ↓
hasAidlHalService("android.hardware.audio.core", "IModule")
    ↓
构造服务名: "android.hardware.audio.core.IModule/default"
    ↓
AServiceManager_isDeclared()
    ↓
/dev/binder → servicemanager
    ↓
检查 VINTF manifest 声明
    ↓
返回 true/false

┌─────────────────────────────────────────────────────────────┐
│                      HIDL 服务检测流程                         │
└─────────────────────────────────────────────────────────────┘
audioserver
    ↓
hasHidlHalService("android.hardware.audio", "IDevicesFactory")
    ↓
构造FQN: "android.hardware.audio@7.0::IDevicesFactory"
    ↓
IServiceManager::getTransport(FQN, "default")
    ↓
/dev/hwbinder → hwservicemanager
    ↓
查询已注册服务列表
    ↓
返回 Transport 类型（HWBINDER/PASSTHROUGH/EMPTY）
```

### Step 3️⃣: 动态加载对应版本的包装库

```cpp
@d:\code\Audio相关\framework\libaudiohal\FactoryHal.cpp#73-101
bool createHalService(const AudioHalVersionInfo& version, bool isDevice, void** rawInterface) {
    // 构造库名：libaudiohal@7.0.so
    const std::string libName = "libaudiohal@" + version.toVersionString() + ".so";
    
    // 工厂函数名：createIDevicesFactory
    const std::string factoryFunctionName = 
        isDevice ? "createIDevicesFactory" : "createIEffectsFactory";
    
    // 动态加载库
    handle = dlopen(libName.c_str(), RTLD_LAZY);
    
    // 获取工厂函数指针
    void* (*factoryFunction)();
    *(void **)(&factoryFunction) = dlsym(handle, factoryFunctionName.c_str());
    
    // 调用工厂函数
    *rawInterface = (*factoryFunction)();
    return true;
}
```

### Step 4️⃣: 工厂函数入口（libaudiohal@7.0.so）

```cpp
@d:\code\Audio相关\framework\libaudiohal\impl\DevicesFactoryHalEntry.cpp#21-24
extern "C" __attribute__((visibility("default"))) 
void* createIDevicesFactory() {
    return createIDevicesFactoryImpl();  // 转发到具体实现
}
```

### Step 5️⃣: 根据版本类型创建具体实现

**AIDL 版本：**
```cpp
@d:\code\Audio相关\framework\libaudiohal\impl\DevicesFactoryHalAidl.cpp#224-227
extern "C" __attribute__((visibility("default"))) 
void* createIDevicesFactoryImpl() {
    // 创建 AIDL 包装器，连接到 servicemanager 中的服务
    return new DevicesFactoryHalAidl(getServiceInstance<IConfig>("default"));
}
```

**HIDL 版本：**
```cpp
// DevicesFactoryHalHidl.cpp
void* createIDevicesFactoryImpl() {
    // 连接到 hwservicemanager 中的服务
    sp<IDevicesFactory> service = IDevicesFactory::getService();
    return new DevicesFactoryHalHidl(service);
}
```

### Step 6️⃣: 通过 ServiceManager 获取远程服务

#### 🔵 AIDL 服务获取详解

```cpp
// 步骤1: 获取 IConfig 服务实例
template <class T>
std::shared_ptr<T> getServiceInstance(const std::string& instance) {
    // 构造服务名：package.interface/instance
    const std::string serviceName = std::string(T::descriptor) + "/" + instance;
    // 例如：android.hardware.audio.core.IConfig/default
    
    // 使用 AIDL NDK API 等待服务
    ::ndk::SpAIBinder binder(AServiceManager_waitForService(serviceName.c_str()));
    
    if (binder.get() == nullptr) {
        ALOGE("Failed to get AIDL service %s", serviceName.c_str());
        return nullptr;
    }
    
    // 从 Binder 创建接口代理
    return T::fromBinder(binder);
}
```

#### 🔵 HIDL 服务获取详解

```cpp
// 步骤1: 获取 HIDL 服务
sp<IDevicesFactory> service = IDevicesFactory::getService("default");
// 内部通过 hwservicemanager 查找并返回 Binder 代理对象
```

---

## 📊 关键数据流

### 🔽 服务端：实现库加载

```
android.hardware.audio.service (可执行文件)
    │
    ├─ dlopen("android.hardware.audio@7.0-impl.so")
    │       │
    │       └─ 包含: DevicesFactory.cpp
    │               ├─ class DevicesFactory : public IDevicesFactory
    │               └─ HIDL_FETCH_IDevicesFactory() { return new DevicesFactory(); }
    │
    └─ registerAsService()
            │
            └─ hwservicemanager 注册
                "android.hardware.audio@7.0::IDevicesFactory/default"
```

### 🔼 客户端：包装库加载

```
audioserver (可执行文件)
    │
    ├─ dlopen("libaudiohal@7.0.so")
    │       │
    │       └─ 包含: DevicesFactoryHalHidl.cpp
    │               ├─ class DevicesFactoryHalHidl : public DevicesFactoryHalInterface
    │               └─ createIDevicesFactory() { return new DevicesFactoryHalHidl(...); }
    │
    └─ IDevicesFactory::getService()
            │
            └─ 从 hwservicemanager 获取
                "android.hardware.audio@7.0::IDevicesFactory/default"
                返回 Binder 代理对象
```

---

## 🎯 完整时序图

```
┌──────────┐        ┌──────────────┐       ┌──────────────┐       ┌──────────┐
│  Init    │        │ audio.service│       │hwservicemanager│      │audioserver│
└────┬─────┘        └──────┬───────┘       └──────┬───────┘       └─────┬────┘
     │                     │                      │                     │
     │ 1. 启动服务           │                      │                     │
     ├──────────────────>│                      │                     │
     │                     │                      │                     │
     │                     │ 2. dlopen(@7.0-impl.so)                   │
     │                     ├────────┐             │                     │
     │                     │        │             │                     │
     │                     │<───────┘             │                     │
     │                     │                      │                     │
     │                     │ 3. registerAsService()                     │
     │                     ├─────────────────────>│                     │
     │                     │                      │                     │
     │                     │ 4. 服务已注册         │                     │
     │                     │<─────────────────────┤                     │
     │                     │                      │                     │
     │                     │ 5. joinRpcThreadpool()                     │
     │                     ├────────┐             │                     │
     │                     │ (等待)  │             │                     │
     │                     │        │             │                     │
     │                                            │                     │
     │                                            │ 6. 启动              │
     │                                            │<────────────────────┤
     │                                            │                     │
     │                                            │ 7. hasHalService()  │
     │                                            │ (查询服务是否存在)    │
     │                                            │<────────────────────┤
     │                                            │                     │
     │                                            │ 8. YES (7.0 存在)   │
     │                                            ├────────────────────>│
     │                                            │                     │
     │                                            │ 9. dlopen(libaudiohal@7.0.so)
     │                                            │                     ├──┐
     │                                            │                     │  │
     │                                            │                     │<─┘
     │                                            │                     │
     │                                            │ 10. getService()    │
     │                                            │<────────────────────┤
     │                                            │                     │
     │                                            │ 11. Binder 代理对象  │
     │                                            ├────────────────────>│
     │                                            │                     │
     │                                            │                     │
     │                     │<────────────────────────────────────────────┤
     │                     │        12. IPC 调用 (通过 Binder)           │
     │                     ├────────────────────────────────────────────>│
     │                     │                      │                     │
```

---

## 📋 总结对比表

| 阶段 | 服务端 (audio.service) | 客户端 (audioserver) |
|------|----------------------|---------------------|
| **1. 初始化** | Init 启动服务进程 | AudioFlinger 初始化 |
| **2. 版本选择** | 从 7.1→7.0→6.0 依次尝试 | 从 AIDL→HIDL 7.1→7.0→6.0 依次检测 |
| **3. 库加载** | `dlopen("@7.0-impl.so")` | `dlopen("libaudiohal@7.0.so")` |
| **4. 工厂函数** | `HIDL_FETCH_IDevicesFactory()` | `createIDevicesFactory()` |
| **5. 实例创建** | `new DevicesFactory()` | `new DevicesFactoryHalHidl()` |
| **6. 服务管理** | `registerAsService()` 注册 | `getService()` 查询 |
| **7. Binder 类型** | hwservicemanager (HIDL) | hwservicemanager (HIDL) |
| **8. 通信方式** | 等待客户端连接 | 通过 Binder 代理调用 |

---

## ✅ 核心要点

1. **双向动态加载**：服务端和客户端都使用 `dlopen` 动态加载对应版本的库
2. **版本协商**：客户端遍历版本列表，找到服务端已注册的最新版本
3. **解耦设计**：可执行文件本身不包含具体实现，只负责加载和桥接
4. **兼容性保证**：一个通用的 service 可以支持多个 HAL 版本
5. **Passthrough 模式**：服务端直接加载实现库并注册，无需额外的 adapter 层

这种设计实现了**完美的版本兼容性和升级灵活性**！🚀