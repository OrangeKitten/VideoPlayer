- 24/08/31
  - 添加 `demux` 模块
  - 添加 `packet` 队列

- 24/09/01
  - 添加 `decode` 模块,目前可以解析出来PCM数据
  - 添加SDL audio_callback线程目前看线程没有跑
  - 解码后的数据也存入到队列中，目前有四个队列分别是AVPacket (audio\video) AVFrame (audio\video)

- 24/09/02 - 24/09/07- - 24/09/02 - 24/09/07
  - 添加`resample`  模块，目前是把resample后的数据写死为format 16bit\channel 2\nb_sample 1024\sample_rat 48hz
  - 目前audio 解码前后的队列都是无阻塞的，也就是说他会把文件中的全部数据一口气都Push进来，这样会导致内存占用较大，下个版本会对其进行优化
  - 该版本支持音频的播放

- 24/09/17
    - dump video数据可以正常播放   
    - 实现阻塞队列 packet queue 限制25，video frame queue 限制3 video frame queue限制8   

- 24/09/23
    - 当前版本可以当场播放，videorender 在主线程播放，并且可以接受SDLEvent
    
- 24/10/04- 24/10/04
    - 创建了ringbuff代替pcm queue,目的是让SDL callback 每次可以获取到想要的数据大小，从而保持audio的正常播放

- 24/11-12
    - 增加avsync逻辑，以audio pts更新锚点，根据video pts 与前后两帧的时间间隔来实现延时与丢帧逻辑
    - 更新消息队列数据结构，使用deque实现peeknext功能
    - 增加pause功能
- 25/01/05
   - 使用QML添加播放界面，目前实现打开文件夹、播放、暂停、停止功能
