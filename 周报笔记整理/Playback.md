# 1

问题1： ·没有传入usage  怎么匹配输出流

首先是会找到一个符合的audio_attributes_t 这里面会包含一个usage

AudioPolicyManager::getAudioAttributes

->
![[Pasted image 20260215170925.png]]
ProductStrategy::getAttributesForStreamType


要从mAttributesVector找到对应的streamtype 接下来看看

mAttributesVector哪里来的 看了一圈发现 是从EngineDefaultConfig.h中的gOrderedStrategies中解析出来的

![[Pasted image 20260215170936.png]]

可以看到 他会默认 拿到第一个usage 这里面对于AUDIO_STREAM_MUSIC而言拿到的audio_attributes_t的是AUDIO_USAGE_MEDIA

对于车载是使用动态路由的（AudioPolicy）

# 2
问题2： mix流如何确定音频输出格式

这个应该是在开机的时候创建输出流的时候就已经定好了，是根据xml文件中

![[Pasted image 20260215170946.png]]

的profile定好的，如果又多个profile应该会找质量最好的pcm输出打开， 有可能是根据xml的顺序来实现的，所以必须将同一个mixPort下的profile按照保真度从高到低的顺序列出。
![[Pasted image 20260215171010.png]]


# 3
#Android音频 
# ![[Pasted image 20260215171024.png]]

 我在这里贴一下图 在 standbyDelay期间是一直会写入空数据的，这个行为是aosp所期望的。 目的可能是兼容不同hal层的行为 保证数据的完整性 或者并且可能出现的爆破音 之类的清理。  已跟启友沟通了，给他提供了一个方案，多设置一个flag ，然后根据standby的状态来设置该flag，来保证只有在 输出通道第一次打开的时候会通知app当前音频的格式。