#ifndef __AUDIO_RESAMPLE_H__
#define __AUDIO_RESAMPLE_H__
#include "file_dump.h"
#include <inttypes.h>
#include <memory>

extern "C" {
#include "libavutil/audio_fifo.h"
#include "libavutil/opt.h"
#include "libswresample/swresample.h"
}
class AudioReSample {
private:
  // 重采样的参数
  typedef struct audio_resampler_params {
    // 输入参数
    enum AVSampleFormat src_sample_fmt;
    int src_sample_rate;
    uint64_t src_channel_layout;
    uint64_t src_channel_number_;
    // 输出参数
    enum AVSampleFormat dst_sample_fmt;
    int dst_sample_rate;
    uint64_t dst_channel_layout;
    uint64_t dst_channel_number_;
  } audio_resampler_params_t;
  struct SwrContext *swr_ctx_;
  uint8_t **resampled_data_; // 用来缓存重采样后的数据
  int dst_linesize_;
  int dst_nb_channels_;
  int max_dst_nb_samples_;
  audio_resampler_params_t resampler_params_;
  AVAudioFifo *audio_fifo; // 采样点的缓存
  std::unique_ptr<FileDump> dump_file_;

public:
  AudioReSample();
  ~AudioReSample();
  int Init(int inSampleRate, int inChannelLayout, AVSampleFormat inFormat,
           int outSampleRate, int outChannelLayout, AVSampleFormat outFormat);
  int audio_resampler_send_frame(AVFrame *frame, AVFrame *frame_resample);
  //  AVFrame *audio_resampler_receive_frame(audio_resampler_t *resampler,
  //                                         int nb_samples);
  void audio_resampler_alloc(const audio_resampler_params_t &resampler_params);
  int get_reframe_samplerate();
};
#endif
