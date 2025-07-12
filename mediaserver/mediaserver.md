# MediaServer启动流程
在main_demaiserver.cpp启动的该服务，这部分代码没什么内容分为两部分
- 创建MMediaPlayerService
    创建一个NuPlayerFactory注册到，map容器sFactoryMap中。key是字符串比如NU_PLAYER
- ResourceManagerService
    创建ResourceObserverService对象，然后注册到ResourceManagerService中。
# MediaPlayer播放流程分析
## 使用 MediaPlayer API 播放音视频的流程
```java
        // 初始化MediaPlayer
        // MediaPlayer类位于frameworks/base/media/java/android/media/MediaPlayer.java
        mediaPlayer = new MediaPlayer();
        try {
            // 设置音频资源 (本地文件或网络URL)
            mediaPlayer.setDataSource("https://www.example.com/audio.mp3"); // 网络音频文件
            // mediaPlayer.setDataSource("/sdcard/Music/sample.mp3");       // 本地音频文件
            // 异步准备播放器
            mediaPlayer.setOnPreparedListener(mp ->{
                Log.d(TAG, "MediaPlayer准备完成，开始播放")
                mediaPlayer.start();// 开始播放
            });
            prepareAsync(); // 异步准备
        } catch (Exception e) {
            Log.e("MediaPlayer", "初始化失败：" + e.getMessage());
        }
        //播放进行中
        mediaPlayer.pause(); // 暂停播放
        mediaPlayer.stop(); // 停止播放
        mediaPlayer.prepare(); // 停止后需要重新准备播放器
```
上面就是使用MediaPlayer API的简化的播放流程。MediaPlayer是支持同步准备的，该代码使用了异步准备，目的是为了不会卡住UI，当准备完成之后会触发MediaPlayer中的函数OnPreparedListener，然后开始播放。
有AudioTrack的播放流程不同的是，不需要write。数据的读写都是MediaPlayer控制，这大大减少了APP的开发难度
接下来我们就按照播放流程来分析MediaServer的控制流。
## 控制流
### New MeidaPlaye
```java
MediaPlayer.java
   public MediaPlayer() {
        this(/*context=*/null, AudioSystem.AUDIO_SESSION_ALLOCATE);
    }
    private MediaPlayer(Context context, int sessionId) {
        // 调用父类构造函数,设置默认的音频属性
        super(new AudioAttributes.Builder().build(),
                AudioPlaybackConfiguration.PLAYER_TYPE_JAM_MEDIAPLAYER);

        // 初始化事件处理Handler,优先使用当前线程Looper
        // 如果当前线程没有Looper则使用主线程Looper
        Looper looper;
        if ((looper = Looper.myLooper()) != null) {
            mEventHandler = new EventHandler(this, looper);
        } else if ((looper = Looper.getMainLooper()) != null) {
            mEventHandler = new EventHandler(this, looper);
        } else {
            mEventHandler = null;
        }

        // 创建TimeProvider对象用于管理媒体播放时间戳和同步
        mTimeProvider = new TimeProvider(this);
        // 创建字幕源输入流列表,用于管理外挂字幕文件
        mOpenSubtitleSources = new Vector<InputStream>();

        // 获取Attribution信息,包含调用者身份信息
        // 如果context为null则使用默认Attribution
        AttributionSource attributionSource =
                context == null ? AttributionSource.myAttributionSource()
                        : context.getAttributionSource();
        if (attributionSource.getPackageName() == null) {
            attributionSource = attributionSource.withPackageName("");
        }

        // 初始化native层MediaPlayer
        try (ScopedParcelState attributionSourceState = attributionSource.asScopedParcelState()) {
            // native_setup用于初始化native层MediaPlayer
            // 参数1: 将Java层MediaPlayer对象的弱引用传递给native层,避免内存泄漏
            // 参数2: attributionSourceState.getParcel()获取包含调用者身份信息的Parcel对象
            //        Parcel是Android中用于序列化数据的容器,可以跨进程传输
            // 参数3: 解析音频会话ID,用于音频焦点管理
            native_setup(new WeakReference<>(this), attributionSourceState.getParcel(),
                    resolvePlaybackSessionId(context, sessionId));
        }
        
        // 注册到音频系统
        baseRegisterPlayer(getAudioSessionId());
    }
```
```c++
android_media_MediaPlayer.cpp
android_media_MediaPlayer_native_setup(JNIEnv *env, jobject thiz, jobject weak_this,
                                       jobject jAttributionSource,
                                       jint jAudioSessionId)
{
    sp<MediaPlayer> mp = sp<MediaPlayer>::make(
        attributionSource, static_cast<audio_session_t>(jAudioSessionId));
    // create new listener and give it to MediaPlayer
    sp<JNIMediaPlayerListener> listener = new JNIMediaPlayerListener(env, thiz, weak_this);
    mp->setListener(listener);
    // Stow our new C++ MediaPlayer in an opaque field in the Java object.
    // env是JNIEnv指针,用于调用JNI函数与Java层交互
    // 这里调用setMediaPlayer函数将native层的MediaPlayer对象保存到Java层MediaPlayer对象中
    // 将native层MediaPlayer对象保存到Java层MediaPlayer的mNativeContext成员变量中
    // 通过JNI的SetLongField函数设置,fields.context对应Java层的mNativeContext字段
    setMediaPlayer(env, thiz, mp);
}

```
### SetDataSource
### PrepareAsync/Prepare
### Start
### Pause/Stop
### Release

