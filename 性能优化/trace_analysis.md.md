# getValues 回调延迟分析 (~1s)

  

## 问题描述

  

`ConfigManager::initVehicle()` 调用 `mVehicleService->getValues()` 后，`onGetValues` 回调延迟约 **1.08 秒**才到达。

  

## 抓取命令

  

```bash

# 开始抓取（异步模式，持续跟踪）

atrace -b 16384 --async_start aidl ss hal binder_driver

  

# 停止抓取并导出到文件

atrace --async_stop > /tmp/car_trace.txt

```

  

### 命令解释

  

| 参数 | 含义 |

|------|------|

| `-b 16384` | ring buffer 大小 16MB，防止高并发时日志丢失 |

| `--async_start` | 异步开始模式，抓取期间不阻塞进程 |

| `aidl` | 跟踪 AIDL binder 调用（IVehicle::getValues 等） |

| `ss` | system server 跟踪（SM 进程等） |

| `hal` | HIDL/AIDL HAL 层调用跟踪 |

| `binder_driver` | binder driver 层原始事件（transaction/send_reply 等） |

| `--async_stop` | 停止异步抓取并输出结果 |

  

抓取后用 `cat /tmp/car_trace.txt` 查看，或用 Android Systrace 工具加载分析。

  

## Trace 关键时间轴（caraudioserver pid=17018）

  

| 序号 | 事件 | Transaction | 时间(s) |

|------|------|-------------|---------|

| 1 | `getValues` requestId=1 发出 → VHAL node:458 | 319524 | 406.105038 |

| 2 | `getValues` requestId=2 发出 → VHAL node:458 | 319530 | 406.105344 |

| 3 | `getValues` requestId=3 发出 → VHAL node:458 | 319755 | 406.607816 |

| 4 | `subscribe` 发出 → VHAL node:458 | 320264 | 406.186785 |

| 5 | `getValues` requestId=4 发出 → VHAL node:458 | 320270 | 406.186949 |

| **—** | **~1.08s gap，VHAL 内部处理中 —** | | |

| 6 | **`onGetValues` 回调到达** (server) | node458回调 | 407.187645 |

  

**406.105038 → 407.187645 = ~1.08 秒**

  

## Trace 关键字段解释

  

```

tracing_mark_write: B|17018|AIDL::...::IVehicleCallback::onGetValues::server

```

  

| 字段 | 含义 |

|------|------|

| `B` | Begin，函数入口开始（对应 `E` 为结束） |

| `17018` | 进程号 = caraudioserver |

| `client` | 该进程作为 AIDL 客户端（主动发起调用） |

| `server` | 该进程作为 AIDL 服务端（被动接收回调） |

  

`onGetValues::server` 意味着：**VHAL (proc 1499) 通过 binder 调用了 caraudioserver 的回调函数**。


## LOG
```
	行  23: 	行  4387:   caraudioserver-17018   (  17018) [007] .....   406.105035: tracing_mark_write: B|17018|AIDL::ndk::android.hardware.automotive.vehicle.IVehicle::getValues::client
	行  33: 	行  4411:   caraudioserver-17018   (  17018) [007] .....   406.105341: tracing_mark_write: B|17018|AIDL::ndk::android.hardware.automotive.vehicle.IVehicle::getValues::client
	行  43: 	行  5821:   caraudioserver-17018   (  17018) [003] .....   406.607802: tracing_mark_write: B|17018|AIDL::ndk::android.hardware.automotive.vehicle.IVehicle::getValues::client
	行  53: 	行  5849:   caraudioserver-17018   (  17018) [003] .....   406.608188: tracing_mark_write: B|17018|AIDL::ndk::android.hardware.automotive.vehicle.IVehicle::getValues::client
	行 238: 	行  9037:   caraudioserver-17018   (  17018) [007] .....   407.186946: tracing_mark_write: B|17018|AIDL::ndk::android.hardware.automotive.vehicle.IVehicle::getValues::client
	行 263: 	行  9097:   caraudioserver-17018   (  17018) [007] .....   407.187645: tracing_mark_write: B|17018|AIDL::ndk::android.hardware.automotive.vehicle.IVehicleCallback::onGetValues::server
	行 266: 	行  9100:   caraudioserver-17018   (  17018) [007] .....   407.187683: tracing_mark_write: B|17018|AIDL::ndk::android.hardware.automotive.vehicle.IVehicleCallback::onGetValues::server
	行 269: 	行  9103:   caraudioserver-17018   (  17018) [007] .....   407.187719: tracing_mark_write: B|17018|AIDL::ndk::android.hardware.automotive.vehicle.IVehicleCallback::onGetValues::server
	行 272: 	行  9107:   caraudioserver-17018   (  17018) [007] .....   407.187757: tracing_mark_write: B|17018|AIDL::ndk::android.hardware.automotive.vehicle.IVehicleCallback::onGetValues::server
	行 275: 	行  9111:   caraudioserver-17018   (  17018) [007] .....   407.187791: tracing_mark_write: B|17018|AIDL::ndk::android.hardware.automotive.vehicle.IVehicleCallback::onGetValues::server
	行 294: 	行 12308:   caraudioserver-17018   (  17018) [007] .....   408.127938: tracing_mark_write: B|17018|AIDL::ndk::android.hardware.automotive.vehicle.IVehicleCallback::onGetValues::server
```
## 延迟定位

  

```

ConfigManager::initVehicle()

  → mVehicleService->getValues()  [client: 发出，406.105038s]

    → binder_transaction 送达 VHAL (proc 1499)  [dest_proc=1499]

      → ??? VHAL 内部处理 ~1.08s ???

    ← binder_transaction 回调返回  [407.187645s]

  ← onGetValues 进入 (server 端标记)

```

  

**结论：延迟发生在 VHAL AIDL Service (proc 1499) 内部**，caraudioserver 侧无优化空间。

  

  

## 验证方法

  

修复后重新抓 trace，对比同一 requestId 发出和回调到达的时间差，预期从 ~1.08s 降至 < 500ms。