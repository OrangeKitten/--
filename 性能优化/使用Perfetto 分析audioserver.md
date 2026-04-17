# 工具
[Android Perfetto 系列 2：Perfetto Trace 抓取 · Android Performance](https://www.androidperformance.com/2024/05/21/Android-Perfetto-02-how-to-get-perfetto/)
根据文件推荐使用python脚本抓数据
## record_android_trace.py 脚本
![[record_android_trace.py]]
## 配置文件
![[config.pbtx]]
## 执行命令
python record_android_trace.py -c config.pbtx -o trace_file.perfetto-trace
# 模拟underrun情况
我们在app中写入一下逻辑。
写入500ms数据然后休眠1s中，以此重复。并且在app中加入trace打印，这样方便我们查看trace。
```
Trace.beginSection( "AudioTrack_write");
Trace.endSection();
```

## 500ms写入的情况
![[Pasted image 20260415144517.png]]
- AudioTrack_write代表app写入数据，三次调用一共写入了500ms的数据
- write代表AF像Audiohal写入写入数据操作，可以看到AF是分多次向Audiohal写入数据的（发现每次写入用时大约都是40ms），虽然在第一次跟第二次写入之间有间隔，但是总体上还是连续的。
- nRdy66 代表是这个AF中track还有多少数据没有想入audiohal，可以看到数据量是起起伏伏是符合预期的。因为有app->AF 涨，AF->Audiohal写入跌。。

![[Pasted image 20260415144605.png]]
## sleep 1 s情况
![[Pasted image 20260415144744.png]]

- underrun_gap_0代表第一次休眠1s
- 在休眠的时候可以看到AF write仍然在写数据给Auiohal，这时候数据是AF共享内存中仍然存在的。在写了四次之后开始出现sleep，write跟sleep交替出现代表没有数据，af会休眠，休眠时放了防止CPU占用过高。
- 并且这时候可以看到nRdy66 是0，说明现在没有可写入的数据给Audiohal了。
## 继续写入数据
![[Pasted image 20260415145207.png]]
可以看到在继续写入数据后nRdy66 立刻有了数据。
# 正常情况
![[Pasted image 20260415145306.png]]
- 正常写入数据可以看到write是连续的，没有sleep的情况。
- nRdy67 是起起伏伏的。

# 总结
那么我们如果通过trace来判断当前声音是否无声或者卡断呢。
## 从现象确定了当前无声 
那么看write是否连续如果不连续并且*nRdyX是0 那么就说明app没有写入数据，nRdyX是满的 那么就说明写入数据到Audiohal阻塞。*
## 从现象看卡顿
那就可以看write是否连续，如果不连续并且nRdyX多次出现0 那么说明app写入数据慢。