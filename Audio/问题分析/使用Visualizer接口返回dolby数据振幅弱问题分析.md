# Visualizer 
## demo
```java
MediaPlayer mediaPlayer = new MediaPlayer();
int audioSessionId = mediaPlayer.getAudioSessionId();
Visualizer visualizer = new Visualizer(audioSessionId);
visualizer.setDataCaptureListener(new Visualizer.OnDataCaptureListener() {
    @Override
    public void onWaveFormDataCapture(Visualizer visualizer, byte[] waveform, int samplingRate) {
        // 更新波形数据
        
    }

    @Override
    public void onFftDataCapture(Visualizer visualizer, byte[] fft, int samplingRate) {
        // 此处可处理 FFT 数据（如果需要频谱信息）
    }
}, Visualizer.getMaxCaptureRate() / 2, true, false); // 启用波形捕获
visualizer.setEnabled(true);
```
## 问题背景
从上述demo发现Visualizer会返回Wave与FFT数据，可以使用这两种数据来实现音随律动。 但是在现在开发过生产发现对于双声道数据来说，画面振幅强烈，但是dolby音乐振幅就比较平缓。 可知dolby在我们平台上会强制解码成7.1.2或7.1.4声道，因此与双声道数据在声道上有很大的不同。
带着这一点我们来分析为什么播放dolby类型的音频，画面震动平缓。
## 代码分析
### Visualizer.java
首先看`Visualizer.java` api使用说明如下

- The Visualizer class enables application to retrieve part of the currently playing audio for visualization purpose. It is not an audio recording interface and ***only returns partial and lowquality audio content**. 
- Waveform data: consecutive **8-bit (unsigned) mono** samples by using the
{@link #getWaveForm(byte[])} method
- requency data: 8-bit magnitude FFT by using the {@link #getFft(byte[])} method

从上述log可知Visualizer返回给app的数据是底层调用getWaveForm\getFft接口获取的，这两个接口返回的是单声、8bit的数据。
到这里我们可以猜测，底层做了声道的mix操作。那么就要去看这个mix算法是如何实现的了。
### Mix算法分析
```c++
EffectVisualizer.cpp
int Visualizer_process(
        effect_handle_t self, audio_buffer_t *inBuffer, audio_buffer_t *outBuffer)
{
    ······
        for (inIdx = 0, captIdx = pContext->mCaptureIdx;
         inIdx < sampleLen;
         captIdx++) {
        if (captIdx >= CAPTURE_BUF_SIZE) captIdx = 0; // wrap

#ifdef BUILD_FLOAT
        float smp = 0.f;
        for (uint32_t i = 0; i < pContext->mChannelCount; ++i) {
            smp += inBuffer->f32[inIdx++];
        }
        buf[captIdx] = clamp8_from_float(smp * fscale);
        ALOGE("DAYANG FLOAT mChannelCount = %d",pContext->mChannelCount);
#else
        const int32_t smp = (inBuffer->s16[inIdx] + inBuffer->s16[inIdx + 1]) >> shift;
        inIdx += FCC_2;  // integer supports stereo only.
        buf[captIdx] = ((uint8_t)smp)^0x80;
          ALOGE("DAYANG int");
#endif // BUILD_FLOAT
    }
}
```
上述代码就是mix的混合方案，BUILD_FLOAT有被声明因此mix算法解释如下：
会把各个声道的值相加在一起存放在smp中，然后smp/channel_number 取平均值，这样可以防止数据过大导致画面跳动峰值过高。
### 问题分析
现在我们清楚了mix的算法，那么聚焦到dolby音频为什么会有这种问题呢，原因是dolby音乐并非都是7.1.2或7.1.4的，很多都是5.1声道的，只是dolby解码库强制解码成7.1.2\7.1.4输出，那么多出来的那些声道的数据补0来实现。
到这里原因也就清楚了，传给Visualizer的pcm中只有6个声道是有数据的，但是确实用7.1.2声道取得平均值，这就会导致返回给app的数据过小。

那我们再来分析一下为什么传给AudioTrack参数是6channel而到Visualizer就是10channel。根本原因是AudioTrack接收的参数是MediaExctor解析出来的channel个数，
Visualizer接收的是audio_policy_configuration.xml文件解析出来的参数。
```xml
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
对于dolby_bus而言，数据的声道数根据硬件功能写死的。必须送入7.1.2声道的数据。并且对于Visualizer而言不能判断送入的数据所有声道都有数据，因此Visualizer无法改变mix算法中声道的个数。


