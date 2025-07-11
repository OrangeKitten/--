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



