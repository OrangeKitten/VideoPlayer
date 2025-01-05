#ifndef __DECODEC_H__
#define __DECODEC_H__
#include "packet_deque.h"
#include "type.h"
#include <memory>
#include <thread>
#include "file_dump.h"
#include "audio_resample.h"

#include <SDL.h>
#include "ringbuf.h"
#include <functional>
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/common.h"
#include "libavutil/time.h"
}
class Clock {
public:
  Clock();
  ~Clock();
  void set_clock_at(double pts, int serial, double time);
  void set_clock(double pts, int serial);
  double  get_clock();
  int pause_state_trans(PlayerState state);

public:
    double	pts_;            // 时钟基础, 当前帧(待播放)显示时间戳，播放后，当前帧变成上一帧
    // 当前pts与当前系统时钟的差值, audio、video对于该值是独立的
    double	pts_drift_;      // clock base minus time at which we updated the clock
    // 当前时钟(如视频时钟)最后一次更新时间，也可称当前时钟时间
    double	last_updated_;   // 最后一次更新的系统时钟
    double	speed_;          // 时钟速度控制，用于控制播放速度
    // 播放序列，所谓播放序列就是一段连续的播放动作，一个seek操作会启动一段新的播放序列
    int	serial_;             // clock is based on a packet with this serial
    int	paused_;             // = 1 说明是暂停状态
    // 指向packet_serial
    int *queue_serial_;      /* pointer to the current packet queue serial, used for obsolete clock detection */
  private:
    std::mutex mtx_;
};
typedef struct Frame {
    AVFrame		*frame;         // 指向数据帧
   // AVSubtitle	sub;            // 用于字幕
   // int		serial;             // 帧序列，在seek的操作时serial会变化
    double		pts;            // 时间戳，单位为秒
    double		duration;       // 该帧持续时间，单位为秒
    //int64_t		pos;            // 该帧在输入文件中的字节位置
    // int		width;              // 图像宽度
    // int		height;             // 图像高读
    // int		format;             // 对于图像为(enum AVPixelFormat)，
    // // 对于声音则为(enum AVSampleFormat)
    // AVRational	sar;            // 图像的宽高比（16:9，4:3...），如果未知或未指定则为0/1
    // int		uploaded;           // 用来记录该帧是否已经显示过？
    // int		flip_v;             // =1则垂直翻转， = 0则正常播放
    int eof;     //1 为结束帧
} Frame;
class Decodec
{
public:
  Decodec();
  ~Decodec();
  Ret setVolume(int volume);
  void setAudioQueue(std::shared_ptr<PacketQueue> pkt_queue);
  void setVideoQueue(std::shared_ptr<PacketQueue> pkt_queue);
  Ret Init();
  void StartDecode();
  Ret setAudioAvCodecInfo(AVCodecParameters *dec);
  Ret setVideoAvCodecInfo(AVCodecParameters *dec);
  void DumpAudioCodecInfo();
  void DumpvideoCodecInfo();
  void VideoRendor();
  #if 0
  void VideoRendor0();
  #endif  
  void set_player_state(PlayerState player_state);
  PlayerState get_player_state();
  Ret set_volume(int volume);
  int get_volume();
  Ret set_mute(bool mute);
  bool get_mute();
  int get_audio_decode_frame();
  void set_video_timebase(AVRational tb);
  void set_audio_timebase(AVRational tb);
  void set_video_frame_rate(AVRational frame_rate);
  void Start_Video_Rendor();
  void PushNullFrame();
  void set_video_frame_render_callback(const WriteYUVDataCallback &cb) ;
private:
  void AudioThread();
  void VideoThread();
  Ret InitAudio();
  Ret InitVideo();
  Ret DecodeAudio(AVPacket *audio_pkt);
  Ret DecodeVideo(AVPacket *video_pkt);
  Ret InitSDL();
  Ret InitAudioSDL();
  Ret InitVideoSDL();
  AVFrame *AllocOutFrame();
  AVSampleFormat TransforSDLFormattoFFmpeg(int SDL_format);
   void video_display(AVFrame *video_frame);
   Ret video_refresh( );
   double vp_duration(Frame *vp, Frame *nextvp);
   double compute_target_delay(double delay);
   


private:
  std::shared_ptr<PacketQueue> audio_pkt_queue_;
  std::shared_ptr<PacketQueue> video_pkt_queue_;

  std::unique_ptr<std::thread> audio_decode_thread_;
  std::unique_ptr<std::thread> video_decode_thread_;

  std::unique_ptr<std::thread> video_rendor_thread_;
  AVCodecParameters *aduio_codec_info_;
  AVCodecParameters *video_codec_info_;

  AVCodecContext *audio_decodec_ctx_;
  AVCodecContext *video_decodec_ctx_;

  AVCodec *audio_decodec_;
  AVCodec *video_decodec_;

  // AVFrame *video_frame_;

  Frame audio_frame_;
  // Frame audio_frame_resample_;
  Frame last_video_frame_; //上一次显示的videoframe
  double frame_timer;             // 记录最后一帧播放的时刻

  int sdl_flag_;
  // SDL Audio Param
  SDL_AudioSpec want_audio_spec_;
  SDL_AudioDeviceID audio_dev;
  // SDL Video Param
  int default_window_width_;
  int default_window_height_;
  int alwaysontop_; // 始终置顶
  int borderless_;  // 无边框
  SDL_Window *window_;
  SDL_Renderer *renderer_;
  SDL_RendererInfo renderer_info_ ;
  SDL_Texture *video_texture_;
  std::unique_ptr<AudioReSample> audio_resample_;

  PlayerState player_state_;
  std::mutex mtx_;

  int frame_drops_late;// 丢弃视频frame计数
  WriteYUVDataCallback write_yuv_data_cb_;


  // clock
  
  AVRational audio_tb_;      // timebase
  AVRational video_tb_;      // timebase


AVRational video_frame_rate_;
public:
  std::shared_ptr<PacketQueue> audio_frame_queue_;
  std::shared_ptr<RingBuffer> audio_ringbuf_;
  std::shared_ptr<PacketQueue> video_frame_queue_;
  std::unique_ptr<FileDump> dump_file_;
  //clock
  double current_audio_clock_; // 当前frame pts+ 持续时间  单位ms
  double audio_callbacl_time_;
  double remaining_time_;      //video比audio慢，需要下一帧需要延时显示的时间
  std::unique_ptr<Clock> audio_clock_;
  std::unique_ptr<Clock> video_clock_;  // 按照video同步的时候才会使用
  int volume_;
  bool mute_;

  SDL_AudioSpec spec_;

  int	bytes_per_sec;          // 一秒时间的字节数，比如采样率48Khz，2 channel，16bit，则一秒48000*2*16/8=192000
};
#endif
