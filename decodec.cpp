#include "decodec.h"
#include <chrono>
#include <cmath>
#include <sys/time.h>
#include <time.h>
extern "C"
{
#include "log.h"
}

#define MAX_AUDIO_FRAME_SIZE 96000 // 50 ms of 48khz 16-bit audio 2channel

/* Minimum SDL audio buffer size, in samples. */
#define SDL_AUDIO_MIN_BUFFER_SIZE 512
/* Calculate actual buffer size keeping in mind not cause too frequent audio
 * callbacks */
#define SDL_AUDIO_MAX_CALLBACKS_PER_SEC 30
static void sdl_audio_callback(void *opaque, Uint8 *stream, int len);
#define AUDIOFRAME 8
#define VIDEOFRAME 8
// #define AV_NOSYNC_THRESHOLD 10.0
/* polls for possible required screen refresh at least this often, should be
 * less than 1/fps */
#define REFRESH_RATE 20

/* no AV sync correction is done if below the minimum AV sync threshold */
#define AV_SYNC_THRESHOLD_MIN 40 // 40ms
/* AV sync correction is done if above the maximum AV sync threshold */
#define AV_SYNC_THRESHOLD_MAX 100 // 100ms
/* If a frame duration is longer than this, it will not be duplicated to
 * compensate AV sync */
#define AV_SYNC_FRAMEDUP_THRESHOLD 40
/* no AV correction is done if too big error */
#define AV_NOSYNC_THRESHOLD 10000 // 10s

static FILE *file = fopen("log.txt", "wb+");
static void print_current_time_with_ms(std::string str);
Decodec::Decodec()
{

  log_add_fp(file, LOG_TRACE);
  dump_file_ = std::make_unique<FileDump>("decode_video.yuv");
  audio_clock_ = std::make_unique<Clock>();
  video_clock_ = std::make_unique<Clock>();

  if (!(audio_frame_.frame = av_frame_alloc()))
  {
    log_error("Could not allocate audio audio_frame_\n");
  }
  // if (!(video_frame_ = av_frame_alloc())) {
  //   log_error("Could not allocate audio video_frame_\n");
  // }

  aduio_codec_info_ = (AVCodecParameters *)malloc(sizeof(AVCodecParameters));
  video_codec_info_ = (AVCodecParameters *)malloc(sizeof(AVCodecParameters));

  audio_decodec_ctx_ = nullptr;
  video_decodec_ctx_ = nullptr;
  audio_decodec_ = nullptr;
  video_decodec_ = nullptr;
  frame_timer = NAN;
  mute_ = false;
  volume_ = 100;
  current_audio_clock_ = -1;
  renderer_info_ = {0};
  audio_callbacl_time_ = 0;
  remaining_time_ = 0;
  last_video_frame_.duration = NAN;
  last_video_frame_.pts = NAN;
  last_video_frame_.frame = nullptr;
  frame_drops_late = 0;
  player_state_ = PlayerState::PlayerState_Stop;
  play_sample_size_ = 0;
}

Decodec::~Decodec()
{
  log_info("~Decodec in ");
  log_info("SDL_Quit() IN audio_dev = %d",audio_dev);
  
  //SDL_CloseAudioDevice(audio_dev);
  log_info("SDL_Quit() OUT ");
  audio_dev = 0;

  log_info("~audio_decode_thread_ in ");
 if (audio_decode_thread_->joinable())
 {
   audio_decode_thread_->join();
 }
  log_info("~audio_decode_thread_ out ");
  log_info("~video_decode_thread_ in ");
  if (video_decode_thread_->joinable())
  {
    video_decode_thread_->join();
  }
   log_info("~video_decode_thread_ out ");

   log_info("~video_rendor_thread_ in ");
  if (video_rendor_thread_->joinable())
  {
    video_rendor_thread_->join();
  }
     log_info("~video_rendor_thread_ out ");

  free(aduio_codec_info_);
  free(video_codec_info_);
  //  SDL_DestroyTexture(video_texture_);
  //  SDL_DestroyRenderer(renderer_);
  //  SDL_DestroyWindow(window_);


  av_frame_free(&audio_frame_.frame);
  // av_frame_free(&last_video_frame_);
  //  av_frame_free(&audio_frame_resample_);
  fclose(file);
   log_info("~Decodec out ");
}

void Decodec::set_player_state(PlayerState player_state)
{
  log_debug("player state = %d\n", player_state);
  player_state_ = player_state;
  if (player_state_ == PlayerState::PlayerState_Play)
  {
    frame_timer += av_gettime_relative() / 1000 - video_clock_->last_updated_;
    video_clock_->set_clock(video_clock_->get_clock(), 0);
    set_mute(false);
  }
  else if (player_state_ == PlayerState::PlayerState_Pause)
  {
    set_mute(true);
  } else if(player_state_ == PlayerState::PlayerState_Stop) {
    log_info("SDL_CloseAudioDevice in ");
    //SDL_CloseAudioDevice(audio_dev);
    log_info("SDL_CloseAudioDevice out");
  }
  video_clock_->paused_ = audio_clock_->paused_ =
      video_clock_->pause_state_trans(player_state_);
}
PlayerState Decodec::get_player_state()
{
  std::lock_guard<std::mutex> lck(mtx_);
  return player_state_;
}

Ret Decodec::InitSDL()
{
  sdl_flag_ = SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER;
  if (SDL_Init(sdl_flag_) < 0)
  {
    log_error("Could not initialize SDL - %s\n", SDL_GetError());
    log_error("(Did you set  the DISPLAY variable?)\n");
    return Ret_ERROR;
  }
  // 使用QML不需要SDL显示 video
  //   if (InitVideoSDL() != Ret_OK)
  //   {
  //     log_error("init sdl video failed");
  //     return Ret_ERROR;
  //   }
#ifdef USE_VIDEO_SDL
  video_rendor_thread_ =
      std::make_unique<std::thread>(&Decodec::VideoRendor, this);
#endif

  // video_rendor_thread_ =
  //   std::make_unique<std::thread>(&Decodec::VideoRendor0, this);
  // InitAuido要在InitVideo之后执行具体原因不清楚
  if (InitAudioSDL() != Ret_OK)
  {
    log_error("init sdl audio failed");
    return Ret_ERROR;
  }

  return Ret_OK;
}

Ret Decodec::InitAudioSDL()
{
  // ToDo 目前写死
  want_audio_spec_.freq = 48000;
  // want_audio_spec_.format = AV_SAMPLE_FMT_S16; // ffmpeg定义
  want_audio_spec_.format = AUDIO_S16LSB; // SDL 定义
  want_audio_spec_.channels = 2;
  want_audio_spec_.silence = 0;
  // want_audio_spec_.samples = 1024;
  want_audio_spec_.samples = FFMAX(
      SDL_AUDIO_MIN_BUFFER_SIZE,
      2 << av_log2(want_audio_spec_.freq / SDL_AUDIO_MAX_CALLBACKS_PER_SEC));

  want_audio_spec_.callback = sdl_audio_callback;
  want_audio_spec_.userdata = this;
  int num = SDL_GetNumAudioDevices(0);
  const char *deviceName = SDL_GetAudioDeviceName(num, 0);
  if ((audio_dev = SDL_OpenAudioDevice(deviceName, 0, &want_audio_spec_, &spec_,
                                       SDL_AUDIO_ALLOW_FREQUENCY_CHANGE)) < 2)
  {
    log_error("can't open audio.\n");
    return Ret_ERROR;
  }
  SDL_PauseAudioDevice(audio_dev, 0);

  bytes_per_sec = av_samples_get_buffer_size(
      NULL, want_audio_spec_.channels, want_audio_spec_.freq,
      TransforSDLFormattoFFmpeg(want_audio_spec_.format), 1);
  log_debug("want_audio_spec_.samples = %d, spec.size  = %d spec.samples = %d "
            "bytes_per_sec = %d",
            want_audio_spec_.samples, spec_.size, spec_.samples, bytes_per_sec);
  return Ret_OK;
}
Ret Decodec::InitVideoSDL()
{
  default_window_width_ = 640;
  default_window_height_ = 480;
  alwaysontop_ = false; // 始终置顶
  borderless_ = false;  // 无边框
  window_ = nullptr;
#if SDL_VERSION_ATLEAST(2, 0, 5)
  sdl_flag_ |= SDL_WINDOW_ALWAYS_ON_TOP;
#else
  log_debug("Your SDL version doesn't support SDL_WINDOW_ALWAYS_ON_TOP.Feature "
            "will be inactive");
#endif
  if (borderless_)
    sdl_flag_ |= SDL_WINDOW_BORDERLESS;
  else
    sdl_flag_ |= SDL_WINDOW_RESIZABLE;

  window_ = SDL_CreateWindow("Big Fat Player", SDL_WINDOWPOS_UNDEFINED,
                             SDL_WINDOWPOS_UNDEFINED, video_codec_info_->width,
                             video_codec_info_->height, SDL_WINDOW_SHOWN);
  // 告诉 SDL 在渲染器缩放纹理时，使用线性插值方法进行过滤
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
  if (window_)
  {
    // 创建renderer
    renderer_ = SDL_CreateRenderer(
        window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer_)
    {
      log_error("Failed to initialize a hardware accelerated renderer: %s\n",
                SDL_GetError());
      renderer_ = SDL_CreateRenderer(window_, -1, 0);
    }
    if (renderer_)
    {
      if (!SDL_GetRendererInfo(renderer_, &renderer_info_))
        log_debug(NULL, AV_LOG_VERBOSE, "Initialized %s renderer.\n",
                  renderer_info_.name);
    }
  }
  video_texture_ = SDL_CreateTexture(
      renderer_, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,
      video_codec_info_->width, video_codec_info_->height);
  if (!window_ || !renderer_ || !renderer_info_.num_texture_formats ||
      !video_texture_)
  {
    log_error("Failed to create window or renderer: %s", SDL_GetError());
    return Ret_ERROR;
  }
  return Ret_OK;
}
#if 0
void Decodec::VideoRendor0()
{

  int ret = 0;
  if (InitVideoSDL() != Ret_OK)
  {
    log_error("init sdl video failed");
    // return Ret_ERROR;
  }
  while (1)
  {
    Frame *video_frame = (Frame *)video_frame_queue_->Pop();
    // dump_file_->WriteVideoYUV420PData(video_frame);
    if (video_frame)
    {

      ret = SDL_UpdateYUVTexture(
          video_texture_, nullptr, video_frame->frame->data[0],
          video_frame->frame->linesize[0], video_frame->frame->data[1],
          video_frame->frame->linesize[1], video_frame->frame->data[2],
          video_frame->frame->linesize[2]);
      // SDL_UpdateTexture(video_texture_, NULL, video_frame->data[0],
      // video_frame->linesize[0]);
      if (ret < 0)
      {
        log_error("Failed to SDL_UpdateYUVTexture: %s", SDL_GetError());
      }

      ret = SDL_RenderClear(renderer_);
      if (ret < 0)
      {
        log_error("Failed to SDL_RenderClear %s", SDL_GetError());
      }

      ret = SDL_RenderCopy(renderer_, video_texture_, nullptr, nullptr);
      if (ret < 0)
      {
        log_error("Failed to render texture: %s", SDL_GetError());
      }

      SDL_RenderPresent(renderer_);
      SDL_Delay(40); // 控制帧率大约为 25fps (1000ms / 25 = 40ms)
      SDL_Event event;
      SDL_PollEvent(&event);
      if (event.type == SDL_QUIT)
      {
        av_frame_free(&video_frame->frame);
      }
      av_frame_free(&video_frame->frame);
      delete video_frame;
    }
  }
}
#endif

  void Decodec::Start_Video_Rendor() {
  video_rendor_thread_ =
      std::make_unique<std::thread>(&Decodec::VideoRendor, this);
  }
void Decodec::VideoRendor()
{
  // 使用QML显示不需要SDL
  #ifdef USE_VIDEO_SDL
    if (InitVideoSDL() != Ret_OK)
    {
      log_error("init sdl video failed");
      // return Ret_ERROR;
    }
#endif
  while (1)
  {
  #ifndef USE_VIDEO_SDL
   if (player_state_ == PlayerState::PlayerState_Stop)
   {
     log_info("PlayerState_Stop exit VideoRendor");
     return;
   }
   #endif
    if (remaining_time_ > 0.0)
    {
      av_usleep((int64_t)(remaining_time_ *
                          1000.0)); // remaining_time <= REFRESH_RATE
    }
    // remaining_time_ = REFRESH_RATE;
    // dump_file_->WriteVideoYUV420PData(video_frame);

    if(video_refresh() == Ret_ERROR) {
        return ;
    }
  }
}
Ret Decodec::video_refresh()
{
   Ret ret = Ret_OK;
  log_debug("\n\n-----------------------------------");
  print_current_time_with_ms("video_refresh in ");
  double last_duration, duration, delay;
  Frame *video_frame = (Frame *)video_frame_queue_->Peek(); // 读取带显示的帧

  if(video_frame->eof ==1) {
      log_info("video_refresh receive null_Frame  exit\n");
      ret = Ret_ERROR;
      return ret;
  }
  last_duration = vp_duration(&last_video_frame_, video_frame);

  delay = compute_target_delay(last_duration);
  log_debug("video_pts = %0.3f last_duration = %0.3f delay = %0.3f\n",
            video_frame->pts, last_duration, delay);
  double time = av_gettime_relative() / 1000.0;
  if (std::isnan(frame_timer))
  {
    frame_timer = time;
  }
  log_debug("frame_timer + delay - time = %0.3f", frame_timer + delay - time);
  if (time < frame_timer + delay && time != frame_timer)
  {
    // 判断是否继续显示上一帧
    //  当前系统时刻还未到达上一帧的结束时刻，那么还应该继续显示上一帧。
    //  计算出最小等待时间
    remaining_time_ = FFMIN(frame_timer + delay - time, REFRESH_RATE);
    log_debug("remaining_time_ = %0.3f", remaining_time_);
    video_display(last_video_frame_.frame);
    return ret;
  }
  remaining_time_ = 0.0;
  // 走到这一步，说明已经到了或过了该显示的时间，待显示帧vp的状态变更为当前要显示的帧
  frame_timer += delay; // 更新当前帧播放时间
  log_debug("delay = %0.3f frame_timer = %0.3f ftime = %0.3f diff = %0.3f\n",
            delay, frame_timer, time, time - frame_timer);
  if (delay > 0 && time - frame_timer > AV_SYNC_THRESHOLD_MAX)
  {
    frame_timer = time; // 如果和系统时间差距太大，就纠正为系统时间
  }
  if (!std::isnan(video_frame->pts))
  {
    video_clock_->set_clock(video_frame->pts, 0);
  }

  // 丢帧逻辑
  if (video_frame_queue_->Size() > 1)
  {
    Frame *Next_Frame = (Frame *)video_frame_queue_->PeekNext();
    duration = vp_duration(video_frame, Next_Frame);
    if (time > frame_timer + duration)
    {
      frame_drops_late++;
      log_debug(" frame_timer + duration - time:%0.3f, duration = %0.3f drop "
                "frame = %d\n",
                (frame_timer + duration) - time, duration, frame_drops_late);
      video_frame = (Frame *)video_frame_queue_->Pop();
      if (last_video_frame_.frame != nullptr)
      {
        av_frame_free(
            &last_video_frame_.frame); // 在clone新的数据之前需要释放掉之前的
      }
      last_video_frame_.frame = av_frame_clone(video_frame->frame);
      last_video_frame_.pts = video_frame->pts;
      last_video_frame_.duration = video_frame->duration;
      av_frame_free(&video_frame->frame);
      delete video_frame;
      return ret ;
    }
  }

  video_frame = (Frame *)video_frame_queue_->Pop();

  video_display(video_frame->frame);
  if (last_video_frame_.frame != nullptr)
  {
    av_frame_free(
        &last_video_frame_.frame); // 在clone新的数据之前需要释放掉之前的
  }
  // 克隆当前帧数据
  last_video_frame_.frame = av_frame_clone(video_frame->frame);
  last_video_frame_.pts = video_frame->pts;
  last_video_frame_.duration = video_frame->duration;
  av_frame_free(&video_frame->frame);
  delete video_frame;
  print_current_time_with_ms("video_refresh out ");
}
double Decodec::compute_target_delay(double delay)
{
  double sync_threshold, diff = 0;
  double video_pts = video_clock_->get_clock();
  double audio_pts = audio_clock_->get_clock();
  log_debug("delay = %0.3f video_clock_->get_clock = %0.3f "
            "audio_clock_->get_clock = %0.3f\n",
            delay, video_pts, audio_pts);
  diff = video_pts - audio_pts;
  /* skip or repeat frame. We take into account the
     delay to compute the threshold. I still don't know
     if it is the best guess */
  sync_threshold =
      FFMAX(AV_SYNC_THRESHOLD_MIN, FFMIN(AV_SYNC_THRESHOLD_MAX, delay));
  if (!std::isnan(diff))
  {
    if (diff < -sync_threshold) // 视频已经落后
    {
      delay = FFMAX(0, delay + diff); //  上一帧持续的时间往小的方向去调整
    }
    else if (diff >= sync_threshold && delay > AV_SYNC_FRAMEDUP_THRESHOLD)
    {
      //  delay = 0.2秒
      // diff  = 1秒
      // delay = 0.2 + 1 = 1.2
      // 视频超前
      // AV_SYNC_FRAMEDUP_THRESHOLD是0.1，此时如果delay>0.1,
      // 如果2*delay时间就有点久
      delay = delay + diff; // 上一帧持续时间往大的方向去调整
      // log_debug( "video: delay=%0.3f A-V=%f\n",
      //        delay, -diff);
    }
    else if (diff >= sync_threshold)
    {
      // 上一帧持续时间往大的方向去调整
      // delay = 0.2 *2 = 0.4
      delay =
          2 *
          delay; // 保持在 2 * AV_SYNC_FRAMEDUP_THRESHOLD内, 即是2*0.1 = 0.2秒内
                 //                delay = delay + diff; //
                 //                上一帧持续时间往大的方向去调整
    }
    else
    {
      // 音视频同步精度在 -sync_threshold ~ +sync_threshold
      // 其他条件就是 delay = delay; 维持原来的delay,
      // 依靠frame_timer+duration和当前时间进行对比
    }
  }
  log_debug("video: delay=%0.3f A-V=%f\n", delay, -diff);
  return delay;
}

double Decodec::vp_duration(Frame *vp, Frame *nextvp)
{
  double duration = 0;
  log_debug("nextvp->pts = %0.3f,vp->pts = %0.3f\n", nextvp->pts, vp->pts);
  if (vp != nullptr && !std::isnan(vp->pts))
  {
    duration = nextvp->pts - vp->pts;
    if (std::isnan(duration) || duration <= 0)
    {
      // log_debug("duration2 = %0.3f",vp->duration);
      return vp->duration;
    }
    else
    {
      return duration;
    }
  }
  else
  {
    return nextvp->duration; // 第一帧情况
  }
}

void print_current_time_with_ms(std::string str)
{
  struct timeval tv;
  gettimeofday(&tv, NULL); // 获取当前时间

  // 获取秒和微秒，并计算毫秒
  time_t seconds = tv.tv_sec;
  struct tm *timeinfo = localtime(&seconds);
  int milliseconds = tv.tv_usec / 1000;
  // 格式化时间并打印

  log_debug("%s: \"%02d:%02d:%02d.%03d\",\n", str.c_str(), timeinfo->tm_hour,
            timeinfo->tm_min, timeinfo->tm_sec, milliseconds);
}

void Decodec::video_display(AVFrame *video_frame)
{

  print_current_time_with_ms("video_display");
#ifdef USE_VIDEO_SDL
  if (video_frame)
  {
    int ret = 0;
    ret = SDL_UpdateYUVTexture(video_texture_, nullptr, video_frame->data[0],
                               video_frame->linesize[0], video_frame->data[1],
                               video_frame->linesize[1], video_frame->data[2],
                               video_frame->linesize[2]);
    // print_current_time_with_ms("SDL_UpdateYUVTexture");
    if (ret < 0)
    {
      log_error("Failed to SDL_UpdateYUVTexture: %s", SDL_GetError());
    }

    ret = SDL_RenderClear(renderer_);
    if (ret < 0)
    {
      log_error("Failed to SDL_RenderClear %s", SDL_GetError());
    }
    // print_current_time_with_ms("SDL_RenderClear");
    ret = SDL_RenderCopy(renderer_, video_texture_, nullptr, nullptr);
    if (ret < 0)
    {
      log_error("Failed to render texture: %s", SDL_GetError());
    }
    // print_current_time_with_ms("SDL_RenderCopy");
    SDL_RenderPresent(renderer_);
    // SDL_Delay(40); // 控制帧率大约为 25fps (1000ms / 25 = 40ms)
    //  print_current_time_with_ms("SDL_RenderPresent");
    SDL_Event event;
    SDL_PollEvent(&event);
    if (event.type == SDL_QUIT)
    {
    }
    // print_current_time_with_ms("SDL_PollEvent");
  }
#else
  write_yuv_data_cb_(video_frame);
#endif
}

void Decodec::set_audio_timebase(AVRational tb) { audio_tb_ = tb; }
void Decodec::set_video_timebase(AVRational tb) { video_tb_ = tb; }
void Decodec::set_video_frame_rate(AVRational frame_rate)
{
  video_frame_rate_ = frame_rate;
}

Ret Decodec::Init()
{
  Ret ret = Ret_OK;
  audio_frame_queue_ = std::make_shared<PacketQueue>(AUDIOFRAME, "audioframe");
  audio_ringbuf_ = std::make_shared<RingBuffer>(MAX_AUDIO_FRAME_SIZE);
  video_frame_queue_ = std::make_shared<PacketQueue>(VIDEOFRAME, "vidoeframe");
  ret = InitAudio();
  ret = InitVideo();
  if (InitSDL() != Ret_OK)
  {
    log_error("init sdl failed");
  }

  audio_resample_ = std::make_unique<AudioReSample>();
  audio_resample_->Init(
      aduio_codec_info_->sample_rate,
      av_get_channel_layout_nb_channels(aduio_codec_info_->channel_layout),
      (AVSampleFormat)aduio_codec_info_->format, want_audio_spec_.freq,
      want_audio_spec_.channels,
      TransforSDLFormattoFFmpeg((AVSampleFormat)want_audio_spec_.format));
  return ret;
}
void Decodec::setAudioQueue(std::shared_ptr<PacketQueue> pkt_queue)
{
  audio_pkt_queue_ = std::move(pkt_queue);
}
void Decodec::setVideoQueue(std::shared_ptr<PacketQueue> pkt_queue)
{
  video_pkt_queue_ = std::move(pkt_queue);
}
Ret Decodec::setVolume(int volume) { return Ret_OK; }

void Decodec::StartDecode()
{

  //  DumpAudioCodecInfo();
  video_decode_thread_ =
      std::make_unique<std::thread>(&Decodec::VideoThread, this);
  audio_decode_thread_ =
      std::make_unique<std::thread>(&Decodec::AudioThread, this);
}
void Decodec::PushNullFrame() {
    video_frame_queue_->Clear();
    Frame *video_null_frame = new Frame();
    video_null_frame->eof = 1;
    video_null_frame->frame = av_frame_alloc();
    video_frame_queue_->Push(video_null_frame);

    audio_frame_queue_->Clear();
    Frame *audio_null_frame = new Frame();
    audio_null_frame->eof = 1;
    audio_null_frame->frame = av_frame_alloc();
    audio_frame_queue_->Push(audio_null_frame);
    
}

void Decodec::VideoThread()
{
  PlayerState temp;
  while (1)
  {
    if (player_state_ == PlayerState::PlayerState_Stop)
    {
      log_info("PlayerState_Stop exit VideoThread\n");
      return;
    }
    if (get_player_state() == PlayerState::PlayerState_Pause)
    {
      // std::this_thread::sleep_for(std::chrono::milliseconds(20));
      continue;
    }
    AVPacket *video_pkt = (AVPacket *)video_pkt_queue_->Pop();
    if(video_pkt->data == nullptr) {
        log_info("video Thread exit\n");
        av_packet_free(&video_pkt);
        //PushNullFrame();
        return;
    }
    DecodeVideo(video_pkt);
    av_packet_free(&video_pkt);
  }
}

Ret Decodec::DecodeVideo(AVPacket *video_pkt)
{
  int ret = avcodec_send_packet(video_decodec_ctx_, video_pkt);
  if (ret < 0)
  {
    log_error("Failed to send packet to decoder\n");
    return Ret_ERROR;
  }
  /* read all the output frames (in general there may be any number of them */
  while (ret >= 0)
  {
    Frame *video_frame = new Frame();
    video_frame->eof = 0;
    if (!(video_frame->frame = av_frame_alloc()))
    {
      log_error("Could not allocate audio video_frame_\n");
    }
    ret = avcodec_receive_frame(video_decodec_ctx_, video_frame->frame);

    if (video_frame)
    {
      if (video_frame->frame->pts != AV_NOPTS_VALUE)
      {
        video_frame->pts =
            (double)video_frame->frame->pts * av_q2d(video_tb_) * 1000;
        // 有的码流 没有pkt_duration
        // video_frame->duration = (double)av_q2d(video_tb_) * 1000 *
        // video_frame->frame->pkt_duration;
        video_frame->duration =
            av_q2d((AVRational){video_frame_rate_.den, video_frame_rate_.num}) *
            1000;
        // log_debug("duration3 = %0.3f",video_frame->duration);
        double diff = video_frame->pts - audio_clock_->get_clock();
        // if (!std::isnan(diff) && fabs(diff) < AV_NOSYNC_THRESHOLD)
        // {
        //   av_frame_unref(video_frame->frame);
        //   delete video_frame;
        //   continue;
        // }
      }
    }
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
    {
      av_frame_unref(video_frame->frame);
      delete video_frame;
      return Ret_ERROR;
    }
    else if (ret < 0)
    {
      log_error("Error during decoding\n");
      av_frame_unref(video_frame->frame);
      delete video_frame;
      return Ret_ERROR;
    }
    video_frame_queue_->Push(video_frame);
    //dump_file_->WriteVideoYUV420PData(video_frame->frame);
  }
}
void Decodec::AudioThread()
{
  // std::this_thread::sleep_for(std::chrono::microseconds(200));
  while (1)
  {
    if (player_state_ == PlayerState::PlayerState_Stop)
    {
      log_info("PlayerState_Stop exit AudioThread");
      return;
    }

    if (get_player_state() == PlayerState::PlayerState_Pause)
    {
      // std::this_thread::sleep_for(std::chrono::milliseconds(20));
      continue;
    }
    AVPacket *audio_pkt = (AVPacket *)audio_pkt_queue_->Pop();
    // dump_file_->WriteBitStream(audio_pkt,
    // audio_pkt->size,audio_decodec_ctx_->codec_id);
    if(audio_pkt->data == nullptr) {
        log_info("AudioThread exit");
        av_packet_free(&audio_pkt);
        return;
    }
    DecodeAudio(audio_pkt);
    av_packet_free(&audio_pkt);
  }
}

Ret Decodec::DecodeAudio(AVPacket *audio_pkt)
{
  int ret = avcodec_send_packet(audio_decodec_ctx_, audio_pkt);
  if (ret < 0)
  {
    log_error("Failed to send packet to decoder\n");
    return Ret_ERROR;
  }
  int data_size = 0;

  int channel_index = 0;
  /* read all the output frames (in general there may be any number of them */
  while (ret >= 0)
  {
    // 解析出来的是32bit float planar 小端数据
    ret = avcodec_receive_frame(audio_decodec_ctx_, audio_frame_.frame);
    //     if (ret >= 0) {
    //     AVRational tb = (AVRational){1, audio_frame_.frame->sample_rate}; //
    //     if (audio_frame_.frame->pts != AV_NOPTS_VALUE) {
    //         // 如果frame->pts正常则先将其从pkt_timebase转成{1,
    //         frame->sample_rate}
    //         // pkt_timebase实质就是stream->time_base
    //         audio_frame_.frame->pts = av_rescale_q(audio_frame_.frame->pts,
    //         video_tb_, tb);
    //     }

    // }
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
      return Ret_ERROR;
    else if (ret < 0)
    {
      log_error("Error during decoding\n");
      return Ret_ERROR;
    }
    data_size = av_get_bytes_per_sample(audio_decodec_ctx_->sample_fmt);
    if (data_size < 0)
    {
      /* This should not occur, checking just for paranoia */
      log_error("Failed to calculate data size\n");
      return Ret_ERROR;
    }
    // dump_file_->WritePcmPlanarData(audio_frame_.data, data_size);
    // log_debug("audio_frame nb_samples = %d", audio_frame_.nb_samples);
    Frame *audio_frame_resample_ = new Frame();
    if (audio_frame_resample_ != nullptr)
    {
      if (!(audio_frame_resample_->frame = AllocOutFrame()))
      {
        log_error("Could not allocate audio audio_frame_resample_\n");
      }
    }
    else
    {
      log_error("Could not allocate audio_frame_resample_");
      return Ret_ERROR;
    }

    // // audio_frame_resample_是resample之后的数据，
    int resample_sample_number = audio_resample_->audio_resampler_send_frame(
        audio_frame_.frame, audio_frame_resample_->frame);

    // dump_file_->WritePcmData(audio_frame_resample_->extended_data,
    //                          resample_sample_number * 2 * 2);
//     audio_ringbuf_->push(audio_frame_resample_->extended_data[0],
//                          resample_sample_number * 2 * 2);
    //  std::vector<uint8_t> temp = audio_ringbuf_->pop(resample_sample_number *
    //  2 *1.5); //for test

    // dump_file_->WritePcmData(temp.data(),temp.size());
  //  log_info("audio_thread push in ");
    audio_frame_queue_->Push(audio_frame_resample_);
     //log_info("audio_thread push out ");
  }
}

int Decodec::get_audio_decode_frame()
{
  Frame *temp_frame = (Frame *)audio_frame_queue_->Pop();
    if(temp_frame->eof ==1) {
      log_info("audio sdl receive null frame \n");
      return 0;
  }
  if(aduio_codec_info_->sample_rate!=want_audio_spec_.freq) {
    //说明需要resample，那么resample后的pts值不应该在用之前的audio_tb了
    audio_tb_.den = want_audio_spec_.freq;
    log_info("change audio_tb.den = %d",want_audio_spec_.freq);
  }
  //设置第一帧audio的pts
  if(StartTime_ ==AV_NOPTS_VALUE) {
    StartTime_ = temp_frame->frame->pts * av_q2d(audio_tb_) * 1000;
  }
  CurrentPts_ = temp_frame->frame->pts * av_q2d(audio_tb_) * 1000;
  // printf("temp_frame_pts = %lld temp_frame_nb_sample = %ld\n",
  // temp_frame->pts, temp_frame->nb_samples);
  int frame_size = av_samples_get_buffer_size(
      NULL, temp_frame->frame->channels, temp_frame->frame->nb_samples,
      (AVSampleFormat)temp_frame->frame->format, 1);
  audio_ringbuf_->push(temp_frame->frame->extended_data[0], frame_size);
  if (temp_frame)
  { // 注意要根据AVStream或者AVCodecContext中的time_base来计算pts
    // 不能使用sample_rate来计算。因为有的audio
    // pts不是根据sample_rate传输的
    // audio_tb_ = (AVRational){
    //     1000, temp_frame->frame->sample_rate}; /
    log_info("temp_frame->frame->pts = %ld",temp_frame->frame->pts);
    temp_frame->pts = (double)temp_frame->frame->pts * av_q2d(audio_tb_) * 1000;

    log_debug("temp_frame->frame->pts = %lld temp_frame_pts = %0.3f "
              "temp_frame_nb_sample = %ld\n",
              temp_frame->frame->pts, temp_frame->pts,
              temp_frame->frame->nb_samples);
  }
  double current_frame_duration_time =
      ((double)temp_frame->frame->nb_samples /
       audio_resample_->get_reframe_samplerate()) *
      1000;
  /* update the audio clock with the pts */
  if (temp_frame->pts > 0)
  {
    // 当前frame的pts加上持续的播放时间
    current_audio_clock_ = temp_frame->pts + current_frame_duration_time;
  }
  else
  {
    current_audio_clock_ = -1;
  }

#ifdef DEBUG
  {

    static double last_clock = 0;
    printf("audio: delay=%3f clock=%3f current_frame_duration_time = %3f\n",
           current_audio_clock_ - last_clock, current_audio_clock_,
           current_frame_duration_time);
    last_clock = current_audio_clock_;
  }
#endif
  av_frame_free(&temp_frame->frame);
  delete (temp_frame);
  return frame_size;
}
AVFrame *Decodec::AllocOutFrame()
{
  int ret;
  AVFrame *frame = av_frame_alloc();
  if (!frame)
  {
    return NULL;
  }

  //  frame->nb_samples = want_audio_spec_.samples;
  frame->nb_samples =
      2048; // 临时设置因为不清楚resample之后的nb_sample，先设置一个较大的nb_sample
  frame->channel_layout =
      av_get_default_channel_layout(want_audio_spec_.channels);
  frame->format = TransforSDLFormattoFFmpeg(want_audio_spec_.format);
  frame->sample_rate = want_audio_spec_.freq;
  ret = av_frame_get_buffer(frame, 0);
  if (ret < 0)
  {
    log_error("cannot allocate audio data buffer\n");
    return NULL;
  }
  return frame;
}

Ret Decodec::setAudioAvCodecInfo(AVCodecParameters *dec)
{
  if (aduio_codec_info_ == nullptr && dec == nullptr)
  {
    return Ret_ERROR;
  }
  memcpy(aduio_codec_info_, dec, sizeof(AVCodecParameters));
  return Ret_OK;
}
Ret Decodec::setVideoAvCodecInfo(AVCodecParameters *dec)
{
  if (video_codec_info_ == nullptr && dec == nullptr)
  {
    return Ret_ERROR;
  }
  memcpy(video_codec_info_, dec, sizeof(AVCodecParameters));
  return Ret_OK;
}

void Decodec::DumpAudioCodecInfo()
{
  log_debug("\n----- Audio info:\n");
  // sample_rate: 音频编解码器的采样率，单位为Hz
  log_debug("samplerate:%dHz\n", aduio_codec_info_->sample_rate);
  // channels: 音频通道数，单声道为1，双声道为2，5.1声道为6
  log_debug("channels:%d\n", aduio_codec_info_->channels);
  // channel_layout:
  // 音频通道布局，用于描述音频通道的布局，例如立体声为2个通道，左声道为1，右声道为2
  log_debug("channel_layout:%d\n", aduio_codec_info_->channel_layout);
  // codec_type: 音频编解码器的类型，通常为AVMEDIA_TYPE_AUDIO
  log_debug("codec_type:%d\n", aduio_codec_info_->codec_type);
}

void Decodec::DumpvideoCodecInfo()
{
  log_debug("\n----- Video info:\n");
  // index: 每个流成分在ffmpeg解复用分析后都有唯一的index作为标识
}

Ret Decodec::InitAudio()
{
  /* find decoder for the stream */
  audio_decodec_ = avcodec_find_decoder(aduio_codec_info_->codec_id);
  if (!audio_decodec_)
  {
    log_error("Failed to find %s codec\n",
              av_get_media_type_string(aduio_codec_info_->codec_type));
    return Ret_ERROR;
  }

  /* Allocate a codec context for the decoder */
  audio_decodec_ctx_ = avcodec_alloc_context3(audio_decodec_);
  if (!audio_decodec_ctx_)
  {
    log_error("Failed to allocate the %s codec context\n",
              av_get_media_type_string(aduio_codec_info_->codec_type));
    return Ret_ERROR;
  }
  /* Copy codec parameters from input stream to output codec context */
  if (avcodec_parameters_to_context(audio_decodec_ctx_, aduio_codec_info_) <
      0)
  {
    log_error("Failed to copy %s codec parameters to decoder context\n",
              av_get_media_type_string(aduio_codec_info_->codec_type));
    return Ret_ERROR;
  }
  /* Init the decoders, with or without reference counting */
  // av_dict_set(&opts, "refcounted_frames", refcount ? "1" : "0", 0);
  if (avcodec_open2(audio_decodec_ctx_, audio_decodec_, nullptr) < 0)
  {
    log_error("Failed to open %s codec\n",
              av_get_media_type_string(aduio_codec_info_->codec_type));
    return Ret_ERROR;
  }
}
Ret Decodec::InitVideo()
{
  /* find decoder for the stream */
  video_decodec_ = avcodec_find_decoder(video_codec_info_->codec_id);
  if (!video_decodec_)
  {
    log_error("Failed to find %s codec\n",
              av_get_media_type_string(video_codec_info_->codec_type));
    return Ret_ERROR;
  }

  /* Allocate a codec context for the decoder */
  video_decodec_ctx_ = avcodec_alloc_context3(video_decodec_);
  if (!video_decodec_ctx_)
  {
    log_error("Failed to allocate the %s codec context\n",
              av_get_media_type_string(video_codec_info_->codec_type));
    return Ret_ERROR;
  }
  /* Copy codec parameters from input stream to output codec context */
  if (avcodec_parameters_to_context(video_decodec_ctx_, video_codec_info_) <
      0)
  {
    log_error("Failed to copy %s codec parameters to decoder context\n",
              av_get_media_type_string(video_codec_info_->codec_type));
    return Ret_ERROR;
  }

  /* Init the decoders, with or without reference counting */
  // av_dict_set(&opts, "refcounted_frames", refcount ? "1" : "0", 0);
  if (avcodec_open2(video_decodec_ctx_, video_decodec_, nullptr) < 0)
  {
    log_error("Failed to open %s codec\n",
              av_get_media_type_string(video_codec_info_->codec_type));
    return Ret_ERROR;
  }
}

static void sdl_audio_callback(void *opaque, Uint8 *stream, int len)
{

  // 获取传入的解码器对象
  Decodec *temp_decodec = (Decodec *)opaque;
  if(temp_decodec->get_player_state() == PlayerState::PlayerState_Stop) {
      log_info("PlayerState_Stop sdl_audio_callback exit"); 
      SDL_memset(stream, 0, len);
      
      //return;
  }
  temp_decodec->audio_callbacl_time_ =
      av_gettime_relative(); // while可能产生延迟;

  // 清空 SDL 的音频缓冲区，防止残留数据
  SDL_memset(stream, 0, len);

  // 用于存储解码后的数据
  static std::vector<uint8_t> audio_buf;
  int audio_buf_size =
      temp_decodec->audio_ringbuf_->occupied_space(); // 缓冲区中剩余数据的大小

  int temp_len = len;
  // 处理每个回调需要的 `len` 数据
  while (len > 0)
  {
    // 如果缓冲区为空，解码一帧数据
    if (audio_buf_size <= 0)
    {
      temp_decodec->get_audio_decode_frame(); // 获取一帧数据填充到ringbuf中
      audio_buf_size = temp_decodec->audio_ringbuf_->occupied_space();
    }

    // 检查缓冲区是否还有数据
    if (audio_buf_size > 0)
    {
      // 计算要写入的字节数
      int to_copy = std::min(len, audio_buf_size);
      // 从环形缓冲区读取数据
      audio_buf = temp_decodec->audio_ringbuf_->pop(to_copy);
      // temp_decodec->dump_file_->WritePcmData(audio_buf.data(),audio_buf.size());
      //  把缓冲区的数据拷贝到 SDL 的音频缓冲区中
      if (temp_decodec->get_mute() == false)
      {
        SDL_MixAudioFormat(stream, audio_buf.data(), AUDIO_S16SYS,
                           audio_buf.size(), temp_decodec->volume_);
      }
      else
      {
        SDL_memset(stream, 0, to_copy);
        SDL_MixAudioFormat(stream, audio_buf.data(), AUDIO_S16SYS,
                           audio_buf.size(), 0);
      }
      // 更新索引和剩余长度
      len -= to_copy;
      audio_buf_size -= to_copy;
      stream += to_copy;
    }
    else
    {
      // 如果缓冲区没有数据，清空音频输出
      SDL_memset(stream, 0, len); // 保证输出为静音
      break;                      // 退出循环
    }
  }

  // 计算时间  注意 这个算法在第一次计算是不准的，因为第一次给sdl赋值的时候
  // sdl中存储audio的内存是空的因此2 *temp_decodec->spec_.size
  // 有点问题不过问题不大
  double temp_time = (double)(1000 * 2 * temp_decodec->spec_.size +
                              temp_decodec->audio_ringbuf_->occupied_space()) /
                     temp_decodec->bytes_per_sec;
  //  log_debug("temp_time = %3f audio_clock = %3f ringbuf_size = %d\n",
  //  temp_time,
  //         temp_decodec->current_audio_clock_,
  //         temp_decodec->audio_ringbuf_->occupied_space());


  temp_decodec->play_sample_size_ +=temp_len;
  //log_info("temp_decodec->play_sample_size_ = %d",temp_decodec->play_sample_size_);

  temp_decodec->audio_clock_->set_clock_at(
      temp_decodec->current_audio_clock_ - temp_time, 0,
      temp_decodec->audio_callbacl_time_ / 1000);

  log_debug("audio_pts = %0.3f", temp_decodec->audio_clock_->get_clock());
}

AVSampleFormat Decodec::TransforSDLFormattoFFmpeg(int SDL_format)
{
  switch (SDL_format)
  {
  case AUDIO_S16SYS:
    return AV_SAMPLE_FMT_S16;
  case AUDIO_S32SYS:
    return AV_SAMPLE_FMT_S32;
  case AUDIO_S16MSB:
    return AV_SAMPLE_FMT_S16;
  case AUDIO_U8:
    return AV_SAMPLE_FMT_U8;
  default:
    return AV_SAMPLE_FMT_NONE;
  }
}
Ret Decodec::set_volume(int volume)
{
  std::lock_guard<std::mutex> lock(mtx_);
  volume_ = volume;
  return Ret_OK;
}
int Decodec::get_volume()
{
  std::lock_guard<std::mutex> lock(mtx_);
  return volume_;
}
Ret Decodec::set_mute(bool mute)
{
  std::lock_guard<std::mutex> lock(mtx_);
  log_debug("mute = %b", mute);
  mute_ = mute;
  return Ret_OK;
}
bool Decodec::get_mute()
{
  std::lock_guard<std::mutex> lock(mtx_);
  log_debug("mute = %b", mute_);
  return mute_;
}
Clock::Clock()
    : pts_(NAN), pts_drift_(NAN), last_updated_(NAN), speed_(1), serial_(0),
      paused_(0), queue_serial_(nullptr) {}
Clock::~Clock() {}

void Clock::set_clock_at(double pts, int serial, double time)
{
  std::lock_guard<std::mutex> lock(mtx_);
  pts_ = pts;           /* 当前帧的pts */
  last_updated_ = time; /* 最后更新的时间，实际上是当前的一个系统时间 */
  pts_drift_ =
      pts_ -
      time; /* 当前帧pts和系统时间的差值，正常播放情况下两者的差值应该是比较固定的，因为两者都是以时间为基准进行线性增长
             */
  serial_ = serial;
  log_debug("pts_ = %3F pts_drift_ = %3F last_updated_ = %3F\n", pts_, pts_drift_,
            last_updated_);
}

void Clock::set_clock(double pts, int serial)
{
  double time = av_gettime_relative() / 1000.0;
  set_clock_at(pts, serial, time);
}

// 获取最后显示一帧的pts+从处理最后一帧到现在的时间
double Clock::get_clock()
{
  std::lock_guard<std::mutex> lock(mtx_);
  // if (*c->queue_serial != c->serial)
  //     return NAN; // 不是同一个播放序列，时钟是无效
  if (paused_)
  {
    return pts_; // 暂停的时候返回的是pts
  }
  else
  {
    double time = av_gettime_relative() / 1000.0;
    return pts_drift_ + time - (time - last_updated_) * (1.0 - speed_);
  }
}

int Clock::pause_state_trans(PlayerState state)
{
  int pause = -1;
  switch (state)
  {
  case PlayerState::PlayerState_Pause:
    pause = 1;
    break;
  case PlayerState::PlayerState_Play:
    pause = 0;
  default:
    pause = -1;
    break;
  }
  return pause;
}

void Decodec::set_video_frame_render_callback(const WriteYUVDataCallback &cb)
{
  write_yuv_data_cb_ = std::move(cb);
}

int  Decodec::get_play_sample_size (){
   std::lock_guard<std::mutex> lck(mtx_);
  // log_info("play_sample_size_ = %d\n",play_sample_size_);
  return play_sample_size_;
}
 int64_t  Decodec::get_current_pts() {
  int64_t CurrentPlayPts_= CurrentPts_ - StartTime_;
  return CurrentPlayPts_;
 }
