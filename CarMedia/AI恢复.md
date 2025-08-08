基于 `CarMediaService.java` 的代码分析，我来总结一下它的主要功能并为重点功能提供Demo。

## 核心功能概述

**CarMediaService** 是车载系统中管理当前活跃媒体源的核心服务，它与 `MediaSessionManager` 不同，确保车载环境中只有一个活跃的媒体源（支持播放和浏览两种模式）。

## 主要功能模块

### 1. **媒体源管理** ⭐⭐⭐
- 设置/获取当前活跃媒体源
- 支持播放模式和浏览模式
- 验证媒体源是否有效（是否有 MediaBrowserService）

### 2. **监听器管理** ⭐⭐⭐
- 注册/注销媒体源变化监听器
- 跨进程回调通知

### 3. **历史记录管理** ⭐⭐
- 保存最近使用的媒体源列表
- 支持媒体源切换和推荐

### 4. **自动播放策略** ⭐⭐
- 4种自动播放配置：从不、总是、按源保留、按前一个保留
- 根据配置自动启动播放

### 5. **用户生命周期管理** ⭐⭐
- 用户切换时的媒体源迁移
- 临时用户的特殊处理

### 6. **包管理集成** ⭐⭐
- 监听应用包的安装/卸载/更新
- 自动回退到备用媒体源

### 7. **电源策略集成** ⭐
- 根据电源策略暂停/恢复播放
- 记忆被暂停前的状态

### 8. **MediaSession集成** ⭐⭐
- 监听活跃 MediaSession
- 自动切换到正在播放的媒体源

## 重点功能Demo

### Demo 1: 媒体源管理基础操作

```java
public class MediaSourceDemo {
    
    // 模拟CarMediaService的核心媒体源管理功能
    public class MediaSourceManager {
        private ComponentName[] mPrimaryMediaComponents = new ComponentName[2]; // 0=播放, 1=浏览
        private final Object mLock = new Object();
        private boolean mIndependentPlaybackConfig = true;
        
        // 设置媒体源
        public void setMediaSource(ComponentName componentName, int mode) {
            synchronized (mLock) {
                if (mPrimaryMediaComponents[mode] != null && 
                    mPrimaryMediaComponents[mode].equals(componentName)) {
                    return; // 相同媒体源，无需更新
                }
                
                ComponentName oldSource = mPrimaryMediaComponents[mode];
                mPrimaryMediaComponents[mode] = componentName;
                
                System.out.println("媒体源变更: " + 
                    (mode == 0 ? "播放" : "浏览") + " 模式 " +
                    "从 " + (oldSource != null ? oldSource.getPackageName() : "null") + 
                    " 切换到 " + componentName.getPackageName());
                
                // 如果不支持独立播放配置，同时更新两个模式
                if (!mIndependentPlaybackConfig) {
                    mPrimaryMediaComponents[1 - mode] = componentName;
                }
                
                // 通知监听器
                notifyListeners(mode);
            }
        }
        
        // 获取当前媒体源
        public ComponentName getMediaSource(int mode) {
            synchronized (mLock) {
                return mPrimaryMediaComponents[mode];
            }
        }
        
        private void notifyListeners(int mode) {
            // 模拟通知监听器
            System.out.println("通知监听器: " + 
                (mode == 0 ? "播放" : "浏览") + "模式媒体源已变更");
        }
    }
    
    // 使用示例
    public static void main(String[] args) {
        MediaSourceManager manager = new MediaSourceDemo().new MediaSourceManager();
        
        // 设置播放媒体源
        ComponentName musicApp = new ComponentName("com.spotify.music", "com.spotify.music.MediaService");
        manager.setMediaSource(musicApp, 0); // 播放模式
        
        // 设置浏览媒体源
        ComponentName podcastApp = new ComponentName("com.google.android.apps.podcasts", "com.google.android.apps.podcasts.MediaService");
        manager.setMediaSource(podcastApp, 1); // 浏览模式
        
        // 获取当前媒体源
        ComponentName currentPlayback = manager.getMediaSource(0);
        ComponentName currentBrowse = manager.getMediaSource(1);
        
        System.out.println("当前播放源: " + currentPlayback.getPackageName());
        System.out.println("当前浏览源: " + currentBrowse.getPackageName());
    }
}
```

### Demo 2: 媒体源监听器系统

```java
public class MediaSourceListenerDemo {
    
    // 媒体源变化监听器接口
    public interface MediaSourceChangedListener {
        void onMediaSourceChanged(ComponentName componentName, int mode);
    }
    
    // 监听器管理系统
    public class MediaSourceListenerManager {
        private Map<Integer, List<MediaSourceChangedListener>> mListeners = new HashMap<>();
        private ComponentName[] mCurrentSources = new ComponentName[2];
        
        public MediaSourceListenerManager() {
            mListeners.put(0, new ArrayList<>()); // 播放模式监听器
            mListeners.put(1, new ArrayList<>()); // 浏览模式监听器
        }
        
        // 注册监听器
        public void registerListener(MediaSourceChangedListener listener, int mode) {
            synchronized (mListeners) {
                mListeners.get(mode).add(listener);
                System.out.println("注册监听器成功 - 模式: " + (mode == 0 ? "播放" : "浏览"));
            }
        }
        
        // 注销监听器
        public void unregisterListener(MediaSourceChangedListener listener, int mode) {
            synchronized (mListeners) {
                mListeners.get(mode).remove(listener);
                System.out.println("注销监听器成功 - 模式: " + (mode == 0 ? "播放" : "浏览"));
            }
        }
        
        // 设置媒体源并通知监听器
        public void setMediaSource(ComponentName source, int mode) {
            synchronized (mListeners) {
                mCurrentSources[mode] = source;
                
                // 通知所有相关监听器
                for (MediaSourceChangedListener listener : mListeners.get(mode)) {
                    try {
                        listener.onMediaSourceChanged(source, mode);
                    } catch (Exception e) {
                        System.err.println("通知监听器失败: " + e.getMessage());
                    }
                }
            }
        }
        
        public ComponentName getCurrentSource(int mode) {
            return mCurrentSources[mode];
        }
    }
    
    // 使用示例
    public static void main(String[] args) {
        MediaSourceListenerManager manager = new MediaSourceListenerDemo().new MediaSourceListenerManager();
        
        // 创建监听器
        MediaSourceChangedListener playbackListener = new MediaSourceChangedListener() {
            @Override
            public void onMediaSourceChanged(ComponentName componentName, int mode) {
                System.out.println("播放监听器收到通知: 新媒体源 = " + 
                    (componentName != null ? componentName.getPackageName() : "null"));
            }
        };
        
        MediaSourceChangedListener browseListener = new MediaSourceChangedListener() {
            @Override
            public void onMediaSourceChanged(ComponentName componentName, int mode) {
                System.out.println("浏览监听器收到通知: 新媒体源 = " + 
                    (componentName != null ? componentName.getPackageName() : "null"));
            }
        };
        
        // 注册监听器
        manager.registerListener(playbackListener, 0);  // 播放模式
        manager.registerListener(browseListener, 1);    // 浏览模式
        
        // 模拟媒体源变化
        ComponentName spotifyApp = new ComponentName("com.spotify.music", "com.spotify.music.MediaService");
        ComponentName youtubeApp = new ComponentName("com.google.android.youtube", "com.google.android.youtube.MediaService");
        
        manager.setMediaSource(spotifyApp, 0);  // 设置播放源
        manager.setMediaSource(youtubeApp, 1);  // 设置浏览源
        
        // 注销监听器
        manager.unregisterListener(playbackListener, 0);
    }
}
```

### Demo 3: 自动播放策略管理

```java
public class AutoplayConfigDemo {
    
    // 自动播放配置常量
    private static final int AUTOPLAY_CONFIG_NEVER = 0;
    private static final int AUTOPLAY_CONFIG_ALWAYS = 1;
    private static final int AUTOPLAY_CONFIG_RETAIN_PER_SOURCE = 2;
    private static final int AUTOPLAY_CONFIG_RETAIN_PREVIOUS = 3;
    
    // 播放状态常量
    private static final int STATE_NONE = 0;
    private static final int STATE_PLAYING = 3;
    private static final int STATE_PAUSED = 2;
    
    public class AutoplayManager {
        private int mPlayOnMediaSourceChangedConfig = AUTOPLAY_CONFIG_RETAIN_PER_SOURCE;
        private int mCurrentPlaybackState = STATE_NONE;
        private Map<String, Integer> mSourcePlaybackStates = new HashMap<>(); // 模拟SharedPreferences
        private ComponentName mCurrentSource;
        
        // 判断是否应该自动播放
        public boolean shouldStartPlayback(int config, ComponentName newSource) {
            switch (config) {
                case AUTOPLAY_CONFIG_NEVER:
                    System.out.println("自动播放策略: 从不自动播放");
                    return false;
                    
                case AUTOPLAY_CONFIG_ALWAYS:
                    System.out.println("自动播放策略: 始终自动播放");
                    return true;
                    
                case AUTOPLAY_CONFIG_RETAIN_PER_SOURCE:
                    String sourceKey = newSource.flattenToString();
                    Integer lastState = mSourcePlaybackStates.get(sourceKey);
                    boolean shouldPlay = (lastState != null && lastState == STATE_PLAYING);
                    System.out.println("自动播放策略: 按源保留 - " + 
                        (shouldPlay ? "该源上次在播放，自动播放" : "该源上次未播放，不自动播放"));
                    return shouldPlay;
                    
                case AUTOPLAY_CONFIG_RETAIN_PREVIOUS:
                    boolean wasPlaying = (mCurrentPlaybackState == STATE_PLAYING);
                    System.out.println("自动播放策略: 保留前一个状态 - " + 
                        (wasPlaying ? "前一个源在播放，自动播放" : "前一个源未播放，不自动播放"));
                    return wasPlaying;
                    
                default:
                    System.err.println("未支持的自动播放配置: " + config);
                    return false;
            }
        }
        
        // 切换媒体源
        public void switchMediaSource(ComponentName newSource, boolean startPlayback) {
            // 保存当前源的播放状态
            if (mCurrentSource != null) {
                mSourcePlaybackStates.put(mCurrentSource.flattenToString(), mCurrentPlaybackState);
            }
            
            ComponentName oldSource = mCurrentSource;
            mCurrentSource = newSource;
            
            // 判断是否应该自动播放
            boolean shouldAutoplay = shouldStartPlayback(mPlayOnMediaSourceChangedConfig, newSource);
            
            if (shouldAutoplay || startPlayback) {
                mCurrentPlaybackState = STATE_PLAYING;
                System.out.println("🎵 开始播放: " + newSource.getPackageName());
            } else {
                mCurrentPlaybackState = STATE_PAUSED;
                System.out.println("⏸️ 暂停状态: " + newSource.getPackageName());
            }
            
            System.out.println("媒体源切换: " + 
                (oldSource != null ? oldSource.getPackageName() : "null") + 
                " -> " + newSource.getPackageName());
        }
        
        // 更新播放状态
        public void updatePlaybackState(int newState) {
            int oldState = mCurrentPlaybackState;
            mCurrentPlaybackState = newState;
            
            if (mCurrentSource != null) {
                mSourcePlaybackStates.put(mCurrentSource.flattenToString(), newState);
            }
            
            System.out.println("播放状态变化: " + stateToString(oldState) + " -> " + stateToString(newState));
        }
        
        private String stateToString(int state) {
            switch (state) {
                case STATE_NONE: return "未知";
                case STATE_PLAYING: return "播放中";
                case STATE_PAUSED: return "已暂停";
                default: return "其他(" + state + ")";
            }
        }
        
        public void setAutoplayConfig(int config) {
            mPlayOnMediaSourceChangedConfig = config;
            System.out.println("自动播放配置已更新: " + configToString(config));
        }
        
        private String configToString(int config) {
            switch (config) {
                case AUTOPLAY_CONFIG_NEVER: return "从不";
                case AUTOPLAY_CONFIG_ALWAYS: return "始终";
                case AUTOPLAY_CONFIG_RETAIN_PER_SOURCE: return "按源保留";
                case AUTOPLAY_CONFIG_RETAIN_PREVIOUS: return "保留前一个";
                default: return "未知";
            }
        }
    }
    
    // 使用示例
    public static void main(String[] args) {
        AutoplayManager manager = new AutoplayConfigDemo().new AutoplayManager();
        
        ComponentName spotify = new ComponentName("com.spotify.music", "com.spotify.music.MediaService");
        ComponentName youtube = new ComponentName("com.google.android.youtube", "com.google.android.youtube.MediaService");
        ComponentName podcast = new ComponentName("com.google.android.apps.podcasts", "com.google.android.apps.podcasts.MediaService");
        
        System.out.println("=== 自动播放策略演示 ===\n");
        
        // 场景1: 始终自动播放
        System.out.println("🔧 设置策略: 始终自动播放");
        manager.setAutoplayConfig(AUTOPLAY_CONFIG_ALWAYS);
        manager.switchMediaSource(spotify, false);
        
        System.out.println();
        
        // 场景2: 从不自动播放
        System.out.println("🔧 设置策略: 从不自动播放");
        manager.setAutoplayConfig(AUTOPLAY_CONFIG_NEVER);
        manager.switchMediaSource(youtube, false);
        
        System.out.println();
        
        // 场景3: 按源保留状态
        System.out.println("🔧 设置策略: 按源保留");
        manager.setAutoplayConfig(AUTOPLAY_CONFIG_RETAIN_PER_SOURCE);
        
        // 先播放Spotify并暂停
        manager.switchMediaSource(spotify, true);
        manager.updatePlaybackState(STATE_PLAYING);
        manager.updatePlaybackState(STATE_PAUSED);
        
        // 切换到YouTube
        manager.switchMediaSource(youtube, false);
        
        // 再切回Spotify，应该恢复暂停状态
        manager.switchMediaSource(spotify, false);
        
        System.out.println();
        
        // 场景4: 保留前一个状态
        System.out.println("🔧 设置策略: 保留前一个状态");
        manager.setAutoplayConfig(AUTOPLAY_CONFIG_RETAIN_PREVIOUS);
        manager.updatePlaybackState(STATE_PLAYING); // Spotify开始播放
        manager.switchMediaSource(podcast, false);  // 切换到Podcast，应该自动播放
    }
}
```

### Demo 4: 包管理集成（应用卸载处理）

```java
public class PackageManagementDemo {
    
    public class PackageUpdateHandler {
        private ComponentName[] mPrimaryMediaComponents = new ComponentName[2];
        private ComponentName[] mRemovedMediaSourceComponents = new ComponentName[2];
        private List<ComponentName> mAvailableMediaSources = new ArrayList<>();
        
        public PackageUpdateHandler() {
            // 初始化可用媒体源
            mAvailableMediaSources.add(new ComponentName("com.spotify.music", "com.spotify.music.MediaService"));
            mAvailableMediaSources.add(new ComponentName("com.google.android.youtube", "com.google.android.youtube.MediaService"));
            mAvailableMediaSources.add(new ComponentName("com.google.android.apps.podcasts", "com.google.android.apps.podcasts.MediaService"));
            mAvailableMediaSources.add(new ComponentName("com.amazon.mp3", "com.amazon.mp3.MediaService"));
            
            // 设置初始媒体源
            mPrimaryMediaComponents[0] = mAvailableMediaSources.get(0); // Spotify播放
            mPrimaryMediaComponents[1] = mAvailableMediaSources.get(1); // YouTube浏览
        }
        
        // 模拟包被移除
        public void onPackageRemoved(String packageName, boolean isReplacing) {
            System.out.println("\n📦 包管理事件: " + packageName + 
                (isReplacing ? " 正在被替换" : " 被卸载"));
                
            for (int mode = 0; mode < 2; mode++) {
                if (mPrimaryMediaComponents[mode] != null && 
                    mPrimaryMediaComponents[mode].getPackageName().equals(packageName)) {
                    
                    System.out.println("⚠️  当前" + (mode == 0 ? "播放" : "浏览") + 
                        "媒体源被影响: " + mPrimaryMediaComponents[mode].getPackageName());
                    
                    if (isReplacing) {
                        // 应用正在更新，找到备用源
                        ComponentName backup = findBackupMediaSource(packageName);
                        if (backup != null) {
                            mRemovedMediaSourceComponents[mode] = mPrimaryMediaComponents[mode];
                            mPrimaryMediaComponents[mode] = backup;
                            System.out.println("🔄 临时切换到备用源: " + backup.getPackageName());
                        } else {
                            System.out.println("❌ 没有找到可用的备用媒体源");
                        }
                    } else {
                        // 应用被彻底卸载
                        ComponentName lastUsed = getLastMediaSource(mode);
                        mPrimaryMediaComponents[mode] = lastUsed;
                        mRemovedMediaSourceComponents[mode] = null;
                        System.out.println("🔄 切换到历史媒体源: " + 
                            (lastUsed != null ? lastUsed.getPackageName() : "默认源"));
                    }
                }
            }
        }
        
        // 模拟包被重新安装
        public void onPackageAdded(String packageName) {
            System.out.println("\n📦 包管理事件: " + packageName + " 被安装/更新完成");
            
            for (int mode = 0; mode < 2; mode++) {
                if (mRemovedMediaSourceComponents[mode] != null && 
                    mRemovedMediaSourceComponents[mode].getPackageName().equals(packageName)) {
                    
                    System.out.println("🔄 恢复之前的媒体源: " + 
                        mRemovedMediaSourceComponents[mode].getPackageName());
                    
                    mPrimaryMediaComponents[mode] = mRemovedMediaSourceComponents[mode];
                    mRemovedMediaSourceComponents[mode] = null;
                }
            }
        }
        
        // 查找备用媒体源（排除指定包名）
        private ComponentName findBackupMediaSource(String excludePackage) {
            for (ComponentName source : mAvailableMediaSources) {
                if (!source.getPackageName().equals(excludePackage)) {
                    return source;
                }
            }
            return null;
        }
        
        // 模拟获取历史媒体源
        private ComponentName getLastMediaSource(int mode) {
            // 简化实现：返回第一个可用的不同于当前的源
            for (ComponentName source : mAvailableMediaSources) {
                if (mPrimaryMediaComponents[mode] == null || 
                    !source.equals(mPrimaryMediaComponents[mode])) {
                    return source;
                }
            }
            return null;
        }
        
        public void printCurrentStatus() {
            System.out.println("\n📊 当前媒体源状态:");
            System.out.println("  播放源: " + 
                (mPrimaryMediaComponents[0] != null ? mPrimaryMediaComponents[0].getPackageName() : "null"));
            System.out.println("  浏览源: " + 
                (mPrimaryMediaComponents[1] != null ? mPrimaryMediaComponents[1].getPackageName() : "null"));
                
            if (mRemovedMediaSourceComponents[0] != null || mRemovedMediaSourceComponents[1] != null) {
                System.out.println("  临时移除的源:");
                if (mRemovedMediaSourceComponents[0] != null) {
                    System.out.println("    播放: " + mRemovedMediaSourceComponents[0].getPackageName());
                }
                if (mRemovedMediaSourceComponents[1] != null) {
                    System.out.println("    浏览: " + mRemovedMediaSourceComponents[1].getPackageName());
                }
            }
        }
    }
    
    // 使用示例
    public static void main(String[] args) {
        PackageUpdateHandler handler = new PackageManagementDemo().new PackageUpdateHandler();
        
        System.out.println("=== 包管理集成演示 ===");
        handler.printCurrentStatus();
        
        // 场景1: Spotify应用更新
        handler.onPackageRemoved("com.spotify.music", true);
        handler.printCurrentStatus();
        
        // 更新完成，恢复
        handler.onPackageAdded("com.spotify.music");
        handler.printCurrentStatus();
        
        // 场景2: YouTube应用被卸载
        handler.onPackageRemoved("com.google.android.youtube", false);
        handler.printCurrentStatus();
        
        // 场景3: 重新安装YouTube
        handler.onPackageAdded("com.google.android.youtube");
        handler.printCurrentStatus();
    }
}
```

## 架构特点总结

1. **服务端架构**: 继承 `ICarMedia.Stub` 提供跨进程服务
2. **线程安全**: 使用 `@GuardedBy("mLock")` 保护关键数据
3. **生命周期集成**: 与用户管理、电源管理深度集成
4. **持久化**: 使用 SharedPreferences 保存状态和历史
5. **事件驱动**: 响应包管理、MediaSession、电源策略等系统事件
6. **策略可配置**: 支持多种自动播放和独立播放策略

这个服务是车载媒体生态系统的核心协调器，确保在复杂的车载环境中提供一致和智能的媒体源管理体验。


基于 `CarMediaService.java` 中的监听器功能，我来详细分析媒体源监听功能的使用场景：

## 主要使用场景

### 1. **车载多屏UI同步** ⭐⭐⭐
**最常见的场景**：车载系统通常有多个显示屏（中控屏、仪表盘、后排娱乐屏等）

```java
// 场景演示：多屏媒体UI同步
public class MultiScreenSyncDemo {
    
    // 模拟不同的显示屏组件
    static class CenterConsoleUI implements MediaSourceChangedListener {
        @Override
        public void onMediaSourceChanged(ComponentName componentName) {
            System.out.println("🖥️ 中控屏UI更新: 显示 " + componentName.getPackageName() + " 的媒体界面");
            // 实际实现中会更新UI显示当前媒体应用的Logo、名称等
        }
    }
    
    static class InstrumentClusterUI implements MediaSourceChangedListener {
        @Override
        public void onMediaSourceChanged(ComponentName componentName) {
            System.out.println("📊 仪表盘更新: 显示 " + componentName.getPackageName() + " 的播放信息");
            // 实际实现中会在仪表盘显示当前歌曲、艺术家等信息
        }
    }
    
    static class RearSeatEntertainmentUI implements MediaSourceChangedListener {
        @Override
        public void onMediaSourceChanged(ComponentName componentName) {
            System.out.println("🎮 后排娱乐屏更新: 切换到 " + componentName.getPackageName() + " 界面");
        }
    }
    
    public static void main(String[] args) {
        // 模拟媒体源管理器
        MediaSourceListenerManager manager = new MediaSourceListenerManager();
        
        // 注册多个屏幕的监听器
        manager.registerListener(new CenterConsoleUI(), 0);
        manager.registerListener(new InstrumentClusterUI(), 0);
        manager.registerListener(new RearSeatEntertainmentUI(), 0);
        
        // 用户切换媒体源 - 所有屏幕都会同步更新
        ComponentName spotify = new ComponentName("com.spotify.music", "com.spotify.music.MediaService");
        manager.setMediaSource(spotify, 0);
        
        System.out.println("---");
        
        ComponentName youtube = new ComponentName("com.google.android.youtube", "com.google.android.youtube.MediaService");
        manager.setMediaSource(youtube, 0);
    }
}
```

### 2. **系统状态栏/通知中心更新** ⭐⭐⭐

```java
// 场景演示：系统状态更新
public class SystemStatusDemo {
    
    static class StatusBarManager implements MediaSourceChangedListener {
        @Override
        public void onMediaSourceChanged(ComponentName componentName) {
            if (componentName != null) {
                System.out.println("🔔 状态栏更新: 当前媒体 - " + componentName.getPackageName());
                // 更新状态栏的媒体图标和信息
                updateMediaNotification(componentName);
            } else {
                System.out.println("🔔 状态栏清除: 无活跃媒体源");
                clearMediaNotification();
            }
        }
        
        private void updateMediaNotification(ComponentName source) {
            // 实际实现：更新Android通知栏的媒体播放控制器
            System.out.println("   - 显示 " + getAppName(source) + " 的播放控制按钮");
            System.out.println("   - 更新媒体源图标");
        }
        
        private void clearMediaNotification() {
            // 清除通知栏的媒体控制器
            System.out.println("   - 隐藏媒体播放控制器");
        }
        
        private String getAppName(ComponentName source) {
            // 简化的应用名称映射
            String packageName = source.getPackageName();
            if (packageName.contains("spotify")) return "Spotify";
            if (packageName.contains("youtube")) return "YouTube Music";
            if (packageName.contains("podcasts")) return "Google Podcasts";
            return packageName;
        }
    }
    
    public static void main(String[] args) {
        MediaSourceListenerManager manager = new MediaSourceListenerManager();
        manager.registerListener(new StatusBarManager(), 0);
        
        // 模拟媒体源变化
        manager.setMediaSource(new ComponentName("com.spotify.music", "com.spotify.music.MediaService"), 0);
        manager.setMediaSource(null, 0); // 清除媒体源
    }
}
```

### 3. **硬件控制集成** ⭐⭐
**方向盘按键、物理旋钮等硬件控制需要知道当前控制哪个媒体应用**

```java
// 场景演示：硬件控制集成
public class HardwareControlDemo {
    
    static class SteeringWheelController implements MediaSourceChangedListener {
        private ComponentName mCurrentMediaSource;
        
        @Override
        public void onMediaSourceChanged(ComponentName componentName) {
            mCurrentMediaSource = componentName;
            if (componentName != null) {
                System.out.println("🎛️ 方向盘控制器: 绑定到 " + componentName.getPackageName());
                // 配置硬件按键的目标应用
                configureHardwareButtons(componentName);
            } else {
                System.out.println("🎛️ 方向盘控制器: 解除绑定");
                disableHardwareButtons();
            }
        }
        
        private void configureHardwareButtons(ComponentName source) {
            System.out.println("   - 播放/暂停按钮 → " + source.getPackageName());
            System.out.println("   - 上一首/下一首按钮 → " + source.getPackageName());
            System.out.println("   - 音量控制 → " + source.getPackageName());
        }
        
        private void disableHardwareButtons() {
            System.out.println("   - 禁用媒体控制按键");
        }
        
        // 模拟硬件按键事件
        public void onPlayPausePressed() {
            if (mCurrentMediaSource != null) {
                System.out.println("▶️ 向 " + mCurrentMediaSource.getPackageName() + " 发送播放/暂停指令");
                // 实际实现：通过MediaController发送播放/暂停命令
            } else {
                System.out.println("❌ 无媒体源，忽略按键");
            }
        }
        
        public void onNextPressed() {
            if (mCurrentMediaSource != null) {
                System.out.println("⏭️ 向 " + mCurrentMediaSource.getPackageName() + " 发送下一首指令");
            }
        }
    }
    
    public static void main(String[] args) {
        MediaSourceListenerManager manager = new MediaSourceListenerManager();
        SteeringWheelController controller = new SteeringWheelController();
        manager.registerListener(controller, 0);
        
        // 设置媒体源
        ComponentName spotify = new ComponentName("com.spotify.music", "com.spotify.music.MediaService");
        manager.setMediaSource(spotify, 0);
        
        // 模拟用户按下方向盘按键
        controller.onPlayPausePressed();
        controller.onNextPressed();
        
        // 切换媒体源
        ComponentName youtube = new ComponentName("com.google.android.youtube", "com.google.android.youtube.MediaService");
        manager.setMediaSource(youtube, 0);
        
        controller.onPlayPausePressed(); // 现在控制YouTube Music
    }
}
```

### 4. **第三方应用集成** ⭐⭐
**车载launcher、音响系统、导航应用等需要感知媒体源变化**

```java
// 场景演示：第三方应用集成
public class ThirdPartyIntegrationDemo {
    
    // 车载Launcher应用
    static class CarLauncher implements MediaSourceChangedListener {
        @Override
        public void onMediaSourceChanged(ComponentName componentName) {
            if (componentName != null) {
                System.out.println("🚗 CarLauncher: 高亮显示 " + componentName.getPackageName() + " 图标");
                // 在主屏幕上高亮当前活跃的媒体应用
                highlightMediaApp(componentName);
            } else {
                System.out.println("🚗 CarLauncher: 清除媒体应用高亮");
                clearMediaHighlight();
            }
        }
        
        private void highlightMediaApp(ComponentName source) {
            System.out.println("   - 添加发光边框到应用图标");
            System.out.println("   - 显示'正在播放'标识");
        }
        
        private void clearMediaHighlight() {
            System.out.println("   - 移除所有媒体应用的高亮效果");
        }
    }
    
    // 导航应用
    static class NavigationApp implements MediaSourceChangedListener {
        @Override
        public void onMediaSourceChanged(ComponentName componentName) {
            if (componentName != null) {
                System.out.println("🗺️ 导航应用: 集成 " + componentName.getPackageName() + " 媒体控制");
                // 在导航界面显示媒体控制小组件
                showMediaWidget(componentName);
            } else {
                System.out.println("🗺️ 导航应用: 隐藏媒体控制组件");
                hideMediaWidget();
            }
        }
        
        private void showMediaWidget(ComponentName source) {
            System.out.println("   - 在导航界面底部显示媒体控制条");
            System.out.println("   - 显示当前播放的歌曲信息");
        }
        
        private void hideMediaWidget() {
            System.out.println("   - 隐藏媒体控制组件，释放屏幕空间");
        }
    }
    
    public static void main(String[] args) {
        MediaSourceListenerManager manager = new MediaSourceListenerManager();
        
        // 第三方应用注册监听
        manager.registerListener(new CarLauncher(), 0);
        manager.registerListener(new NavigationApp(), 0);
        
        // 用户启动音乐应用
        ComponentName spotify = new ComponentName("com.spotify.music", "com.spotify.music.MediaService");
        manager.setMediaSource(spotify, 0);
        
        System.out.println("---");
        
        // 用户关闭音乐
        manager.setMediaSource(null, 0);
    }
}
```

### 5. **语音助手集成** ⭐⭐
**语音助手需要知道当前媒体源来正确处理语音命令**

```java
// 场景演示：语音助手集成
public class VoiceAssistantDemo {
    
    static class VoiceAssistant implements MediaSourceChangedListener {
        private ComponentName mCurrentMediaSource;
        
        @Override
        public void onMediaSourceChanged(ComponentName componentName) {
            mCurrentMediaSource = componentName;
            if (componentName != null) {
                System.out.println("🎤 语音助手: 学习 " + componentName.getPackageName() + " 的控制指令");
                // 根据不同的媒体源，配置不同的语音指令处理逻辑
                configureVoiceCommands(componentName);
            } else {
                System.out.println("🎤 语音助手: 清除媒体控制指令");
                clearVoiceCommands();
            }
        }
        
        private void configureVoiceCommands(ComponentName source) {
            String appName = getAppName(source);
            System.out.println("   - 启用指令: '播放音乐' → 控制 " + appName);
            System.out.println("   - 启用指令: '暂停' → 控制 " + appName);
            System.out.println("   - 启用指令: '下一首' → 控制 " + appName);
            
            // 为特定应用配置专用指令
            if (source.getPackageName().contains("spotify")) {
                System.out.println("   - 启用指令: '播放我的每日推荐' → Spotify专用");
            } else if (source.getPackageName().contains("podcasts")) {
                System.out.println("   - 启用指令: '播放最新一集' → Podcast专用");
            }
        }
        
        private void clearVoiceCommands() {
            System.out.println("   - 禁用所有媒体控制语音指令");
        }
        
        // 模拟语音指令处理
        public void onVoiceCommand(String command) {
            if (mCurrentMediaSource == null) {
                System.out.println("🎤 语音反馈: 没有活跃的媒体应用");
                return;
            }
            
            switch (command.toLowerCase()) {
                case "播放音乐":
                    System.out.println("🎤 执行: 向 " + mCurrentMediaSource.getPackageName() + " 发送播放指令");
                    break;
                case "暂停":
                    System.out.println("🎤 执行: 向 " + mCurrentMediaSource.getPackageName() + " 发送暂停指令");
                    break;
                case "下一首":
                    System.out.println("🎤 执行: 向 " + mCurrentMediaSource.getPackageName() + " 发送下一首指令");
                    break;
                default:
                    System.out.println("🎤 语音反馈: 不支持的指令 '" + command + "'");
            }
        }
        
        private String getAppName(ComponentName source) {
            String packageName = source.getPackageName();
            if (packageName.contains("spotify")) return "Spotify";
            if (packageName.contains("youtube")) return "YouTube Music";
            if (packageName.contains("podcasts")) return "Google Podcasts";
            return packageName;
        }
    }
    
    public static void main(String[] args) {
        MediaSourceListenerManager manager = new MediaSourceListenerManager();
        VoiceAssistant assistant = new VoiceAssistant();
        manager.registerListener(assistant, 0);
        
        // 用户启动Spotify
        ComponentName spotify = new ComponentName("com.spotify.music", "com.spotify.music.MediaService");
        manager.setMediaSource(spotify, 0);
        
        // 模拟语音指令
        assistant.onVoiceCommand("播放音乐");
        assistant.onVoiceCommand("下一首");
        
        System.out.println("---");
        
        // 切换到Podcast应用
        ComponentName podcast = new ComponentName("com.google.android.apps.podcasts", "com.google.android.apps.podcasts.MediaService");
        manager.setMediaSource(podcast, 0);
        
        assistant.onVoiceCommand("播放音乐"); // 现在控制Podcast应用
    }
}
```

### 6. **系统集成和日志记录** ⭐
**系统监控、使用统计、调试诊断等**

```java
// 场景演示：系统集成
public class SystemIntegrationDemo {
    
    static class UsageAnalytics implements MediaSourceChangedListener {
        private long mLastSwitchTime = System.currentTimeMillis();
        
        @Override
        public void onMediaSourceChanged(ComponentName componentName) {
            long currentTime = System.currentTimeMillis();
            long usageDuration = currentTime - mLastSwitchTime;
            
            if (componentName != null) {
                System.out.println("📊 使用统计: 切换到 " + componentName.getPackageName());
                System.out.println("   - 上个应用使用时长: " + usageDuration + "ms");
                
                // 记录使用数据
                recordMediaSourceUsage(componentName, currentTime);
            } else {
                System.out.println("📊 使用统计: 媒体会话结束，使用时长: " + usageDuration + "ms");
            }
            
            mLastSwitchTime = currentTime;
        }
        
        private void recordMediaSourceUsage(ComponentName source, long timestamp) {
            // 实际实现会记录到数据库或发送到分析服务
            System.out.println("   - 记录: " + source.getPackageName() + " @ " + timestamp);
        }
    }
    
    static class SystemLogger implements MediaSourceChangedListener {
        @Override
        public void onMediaSourceChanged(ComponentName componentName) {
            String logMessage = componentName != null ? 
                "MediaSource switched to: " + componentName.flattenToString() :
                "MediaSource cleared";
                
            System.out.println("🔍 系统日志: " + logMessage);
            
            // 实际实现会写入系统日志文件
            writeToSystemLog(logMessage);
        }
        
        private void writeToSystemLog(String message) {
            // 写入到 /data/system/car/media_service.log 等
            System.out.println("   - 写入日志文件: " + message);
        }
    }
    
    public static void main(String[] args) {
        MediaSourceListenerManager manager = new MediaSourceListenerManager();
        manager.registerListener(new UsageAnalytics(), 0);
        manager.registerListener(new SystemLogger(), 0);
        
        // 模拟用户使用行为
        ComponentName spotify = new ComponentName("com.spotify.music", "com.spotify.music.MediaService");
        manager.setMediaSource(spotify, 0);
        
        // 模拟使用一段时间后切换
        try { Thread.sleep(100); } catch (InterruptedException e) {}
        
        ComponentName youtube = new ComponentName("com.google.android.youtube", "com.google.android.youtube.MediaService");
        manager.setMediaSource(youtube, 0);
    }
}
```

## 监听器使用的关键时机

### **注册时机**
- **系统启动时**: 系统UI组件在启动时注册监听器
- **应用启动时**: 第三方应用连接到CarService时注册
- **用户登录时**: 个人化服务在用户登录后注册
- **硬件连接时**: 外设或显示器连接时注册

### **注销时机**
- **应用退出时**: 避免内存泄漏
- **用户切换时**: 清理前一用户的监听器
- **系统关机时**: 优雅关闭
- **权限变更时**: 失去权限时自动注销

## 实际产品中的应用

1. **Tesla**: 媒体源切换时，中控屏、仪表盘、手机App同步更新
2. **BMW iDrive**: 方向盘控制、语音指令根据当前媒体源智能路由
3. **Android Auto**: 手机端媒体应用与车机端UI实时同步
4. **CarPlay**: 多个iOS媒体应用的统一控制和状态同步

这些监听器功能是车载媒体生态系统中各个组件协调工作的基础，确保用户在使用不同输入方式（触屏、语音、硬件按键）时都能获得一致的体验。