# CarMediaService
该文件是CarMediamanager的实现，APP直接调用CarMediamanager.
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

# 代码走读
## init
```java
    public void init() {
        int currentUserId = ActivityManager.getCurrentUser();
        Slog.d(CarLog.TAG_MEDIA, "init(): currentUser=" + currentUserId + " (0表示系统用户/SYSTEM_USER)");
        maybeInitUser(currentUserId);
        setPowerPolicyListener();  //我们不关系电源策略
    }

        private void maybeInitUser(int userId) {
        if (userId == UserHandle.USER_SYSTEM) {
            return;
        }
        if (mUserManager.isUserUnlocked(userId)) {
            initUser(userId);
        } else {
            mPendingInit = true;
        }
    }
```