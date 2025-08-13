# AAudio学习
目前我们可知AAudio对于APM、AF来说也是一个client端类似与AudioTrack的感觉AAudio最优先匹配的是mmap输出流其次是fast输出流。我们接下来的学习已AAudio中的example入手，先看看这个模块是怎么工作的，然后在对源码进行分析。另外我们还要关注一下mmapthread中的内存是如何减少拷贝的。
## AAudio example
这部分代码路径frameworks\av\media\libaaudio\examples
### write_sine
该路径在存在`write_sine_callback.cpp`与`write_sine.cpp`文件，实现功能分别是使用回调与阻塞模式来写入正弦波。
#### write
我们来提取一下用到的AAudio的api。该流程与使用AudioTrack的流程类似。
```c++

        // 1.创建AAudioStreamBuilder
        AAudioStreamBuilder *builder = nullptr;
        result = AAudio_createStreamBuilder(&builder);
        //2.设置AAudioStreamBuilder的参数
        // 设置缓冲区容量（帧数）
        AAudioStreamBuilder_setBufferCapacityInFrames(builder, getBufferCapacity());
        // 设置通道数
        AAudioStreamBuilder_setChannelCount(builder, mChannelCount);
        // 设置设备ID（指定输出/输入设备）
        AAudioStreamBuilder_setDeviceId(builder, mDeviceId);
        // 设置音频数据格式
        AAudioStreamBuilder_setFormat(builder, mFormat);
        // 设置每次数据回调的帧数
        AAudioStreamBuilder_setFramesPerDataCallback(builder, mFramesPerCallback);
        // 设置性能模式（如低延迟、节能等）
        AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_NONE);
        // 设置采样率
        AAudioStreamBuilder_setSampleRate(builder, mSampleRate);
        // 设置共享模式（独占/共享）
        AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
        // 设置流方向为输出
        AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
        // 打开音频流
        AAudioStream *aaudioStream
        //3. 使用AAudioStreamBuilder 创建 AAudioStream
        result = AAudioStreamBuilder_openStream(builder, &aaudioStream);

        // 如果流打开成功，设置缓冲区大小
        if (result == AAUDIO_OK) {
            int32_t sizeInBursts = parameters.getNumberOfBursts(); // 获取突发数
            int32_t framesPerBurst = AAudioStream_getFramesPerBurst(mStraaudioStreameam); // 获取每个突发的帧数
            int32_t bufferSizeFrames = sizeInBursts * framesPerBurst; // 计算总缓冲帧数
            AAudioStream_setBufferSizeInFrames(aaudioStream, bufferSizeFrames); // 设置缓冲区大小
        }
        //4.AAudioStream创建成功后删除builder，释放资源
        AAudioStreamBuilder_delete(builder);
          // 获取流的实际参数
        actualChannelCount = AAudioStream_getChannelCount(aaudioStream);
        actualSampleRate = AAudioStream_getSampleRate(aaudioStream);
        actualDataFormat = AAudioStream_getFormat(aaudioStream);

        framesPerBurst = AAudioStream_getFramesPerBurst(aaudioStream);
            //程序单次调用 AAudioStream_write() 时实际写入的帧数。
        int32_t  framesPerWrite = 0;
        framesPerWrite = framesPerBurst;
        //5.start AAudioStream
        aaudio_result_t result = AAudioStream_requestStart(mStream);
        if (result != AAUDIO_OK) {
            printf("ERROR - AAudioStream_requestStart(output) returned %d %s\n",
                    result, AAudio_convertResultToText(result));
        }
        //6.write data
        //write stream floatData存放的数据 这是一个阻塞调用，会等待直到数据被完全写入或超时。
        AAudioStream_write(aaudioStream, floatData, minFrames, timeoutNanos);

            // 获取并打印XRun(缓冲区欠载/过载)的次数 查看稳定性
        int xRunCount = AAudioStream_getXRunCount(aaudioStream);
        printf("AAudioStream_getXRunCount %d\n", xRunCount);

          aaudio_result_t result = AAudioStream_requestStop(mStream);
        if (result != AAUDIO_OK) {
            printf("ERROR - AAudioStream_requestStop(output) returned %d %s\n",
                   result, AAudio_convertResultToText(result));
        }
        //7.close AAudioStream
        AAudioStream_close(aaudioStream);
```
这个流程分为七部分看起来还是很清晰的。
****
#### callback
大体相同不同在于
```c++
/**
 * AAudio 数据回调函数。
 * 这个函数会在一个高优先级的实时线程中被 AAudio 服务周期性地调用。
 * 你的所有音频处理和数据生成都应该在这里完成。
 *
 * @param stream 我们正在操作的 AAudio 流
 * @param userData 在注册回调时传入的自定义数据指针
 * @param audioData 需要我们填充音频数据的缓冲区
 * @param numFrames 系统请求我们提供的音频帧数
 * @return 必须返回 AAUDIO_CALLBACK_RESULT_CONTINUE，流才会继续播放
 */
aaudio_callback_result_t dataCallback(
        AAudioStream *stream,
        void *userData,
        void *audioData,
        int32_t numFrames) {

    // 将 userData 和 audioData 转换为我们需要的类型
    SineGenerator *sineGenerator = (SineGenerator *)userData;
    float *outputBuffer = (float *)audioData;
    int32_t channelCount = AAudioStream_getChannelCount(stream);

    // 循环生成每一帧的音频数据
    for (int i = 0; i < numFrames; i++) {
        float sampleValue = sinf(sineGenerator->phase);

        // 为所有声道写入相同的采样值
        for (int j = 0; j < channelCount; j++) {
            outputBuffer[i * channelCount + j] = sampleValue;
        }

        // 更新相位
        sineGenerator->phase += sineGenerator->phaseIncrement;
        // 当相位超过 2*PI 时，将其卷绕回来，以防止浮点数精度问题
        if (sineGenerator->phase > 2.0 * M_PI) {
            sineGenerator->phase -= 2.0 * M_PI;
        }
    }

    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}
    // 关键：注册我们的回调函数，并把振荡器作为 userData 传入 sineOscillator是一个结构体方便在dataCallback中使用
    AAudioStreamBuilder_setDataCallback(builder, dataCallback, &sineOscillator);
```
### loopback
### input_monitor
#### monitor
#### callback
## AAudio 源码分析
## mmapthread 内存拷贝