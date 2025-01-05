#include "demux.h"
#include "file_dump.h"
extern "C" {
#include "log.h"
}

Demux::Demux(const char *url) {

  format_ctx_ = nullptr;
  avio_ctx_ = nullptr;
  video_stream_ = nullptr;
  audio_stream_ = nullptr;
  video_stream_index_ = -1;
  audio_stream_index_ = -1;
  read_size_ = 0;
  write_size_ = 0;
  url_ = url;
  frame_rate_ = {0, 0};
  dump_file_ = std::make_unique<FileDump>("demux_audio.aac");

  if (OpenFile() < 0) {
    log_error("OpenFile Failed");
  }
}

int Demux::OpenFile() {
  // AVFormatContext是描述一个媒体文件或媒体流的构成和基本信息的结构体
  // 打开文件，主要是探测协议类型，如果是网络文件则创建网络链接
  int ret = avformat_open_input(&format_ctx_, url_, NULL, NULL);
  if (ret < 0) // 如果打开媒体文件失败，打印失败原因
  {
    char buf[1024] = {0};
    av_strerror(ret, buf, sizeof(buf) - 1);
    log_error("open %s failed:%s\n", url_, buf);
  }

  ret = avformat_find_stream_info(format_ctx_, NULL);
  if (ret < 0) // 如果打开媒体文件失败，打印失败原因
  {
    char buf[1024] = {0};
    av_strerror(ret, buf, sizeof(buf) - 1);
    log_error("avformat_find_stream_info %s failed:%s\n", url_, buf);
  }

  // 打开媒体文件成功
  log_debug("\n==== av_dump_format url_:%s ===\n", url_);
  av_dump_format(format_ctx_, 0, url_, 0);
  log_debug("\n==== av_dump_format finish =======\n\n");
  // url_: 调用avformat_open_input读取到的媒体文件的路径/名字
  log_debug("media name:%s\n", format_ctx_->url);
  // nb_streams: nb_streams媒体流数量
  log_debug("stream number:%d\n", format_ctx_->nb_streams);
  // bit_rate: 媒体文件的码率,单位为bps
  log_debug("media average ratio:%lldkbps\n",
            (int64_t)(format_ctx_->bit_rate / 1024));
  // 时间
  int total_seconds, hour, minute, second;
  // duration: 媒体文件时长，单位微妙
  total_seconds =
      (format_ctx_->duration) / AV_TIME_BASE; // 1000us = 1ms, 1000ms = 1秒
  hour = total_seconds / 3600;
  minute = (total_seconds % 3600) / 60;
  second = (total_seconds % 60);
  // 通过上述运算，可以得到媒体文件的总时长
  log_debug("total duration: %02d:%02d:%02d\n", hour, minute, second);
  log_debug("\n");
  /*
   * 老版本通过遍历的方式读取媒体文件视频和音频的信息
   * 新版本的FFmpeg新增加了函数av_find_best_stream，也可以取得同样的效果
   */
  video_stream_index_ =
      av_find_best_stream(format_ctx_, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
  video_stream_ = format_ctx_->streams[video_stream_index_];
  audio_stream_index_ =
      av_find_best_stream(format_ctx_, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
  audio_stream_ = format_ctx_->streams[audio_stream_index_];
  video_stream_ = format_ctx_->streams[video_stream_index_];
  AUDIO_INFO audio_info;
  audio_info.channels = audio_stream_->codecpar->channels;
  audio_info.format = audio_stream_->codecpar->format;
  audio_info.freq = audio_stream_->codecpar->sample_rate;
  audio_info.samples = audio_stream_->codecpar->frame_size;
  setAudioInfo(audio_info);
  DumpMedioInfo();
}

Demux::~Demux() {
  log_info("~Demux in");

  if (read_packet_thread_->joinable()) {
    read_packet_thread_->join();
  }
  if (format_ctx_)
    avformat_close_input(&format_ctx_);
  log_info("~Demux out");

}

AVRational Demux::getVideoTimeBase() { return video_stream_->time_base; }

AVRational Demux::getAudioTimeBase() { return audio_stream_->time_base; }

AVRational Demux::getVideoFrameRate() {
  return frame_rate_ = av_guess_frame_rate(format_ctx_, video_stream_, NULL);
}

void Demux::DumpMedioInfo() {

  log_debug("----- Audio info:\n");
  // index: 每个流成分在ffmpeg解复用分析后都有唯一的index作为标识
  log_debug("index:%d\n", audio_stream_->index);
  // sample_rate: 音频编解码器的采样率，单位为Hz
  log_debug("samplerate:%dHz\n", audio_stream_->codecpar->sample_rate);
  // codecpar->format: 音频采样格式
  if (AV_SAMPLE_FMT_FLTP == audio_stream_->codecpar->format) {
    log_debug("sampleformat:AV_SAMPLE_FMT_FLTP\n");
  } else if (AV_SAMPLE_FMT_S16P == audio_stream_->codecpar->format) {
    log_debug("sampleformat:AV_SAMPLE_FMT_S16P\n");
  }
  // channels: 音频信道数目
  log_debug("channel number:%d\n", audio_stream_->codecpar->channels);
  // codec_id: 音频压缩编码格式
  if (AV_CODEC_ID_AAC == audio_stream_->codecpar->codec_id) {
    log_debug("audio codec:AAC\n");
  } else if (AV_CODEC_ID_MP3 == audio_stream_->codecpar->codec_id) {
    log_debug("audio codec:MP3\n");
  } else {
    log_debug("audio codec_id:%d\n", audio_stream_->codecpar->codec_id);
  }
  // 音频总时长，单位为秒。注意如果把单位放大为毫秒或者微妙，音频总时长跟视频总时长不一定相等的
  if (audio_stream_->duration != AV_NOPTS_VALUE) {
    int duration_audio =
        (audio_stream_->duration) * av_q2d(audio_stream_->time_base);
    // 将音频总时长转换为时分秒的格式打印到控制台上
    log_debug("audio duration: %02d:%02d:%02d\n", duration_audio / 3600,
              (duration_audio % 3600) / 60, (duration_audio % 60));
  } else {
    log_debug("audio duration unknown");
  }

  log_debug("\n");
  log_debug("----- Video info:\n");
  // index: 每个流成分在ffmpeg解复用分析后都有唯一的index作为标识
  log_debug("index:%d\n", video_stream_->index);
  // time_base: 时基是用于表示时间戳的基本单位，对于视频流通常是1/帧率
  log_debug("time_base:%d/%d\n", video_stream_->time_base.num,
            video_stream_->time_base.den);
  // width 和 height: 分别是视频的宽度和高度，以像素为单位
  log_debug("video width:%d\n", video_stream_->codecpar->width);
  log_debug("video height:%d\n", video_stream_->codecpar->height);
  // code_id 编解码格式
  if (AV_CODEC_ID_H264 == video_stream_->codecpar->codec_id) {
    log_debug("video codec:H264\n");
  } else if (AV_CODEC_ID_MPEG4 == video_stream_->codecpar->codec_id) {
    log_debug("video codec:MPEG4\n");
  } else {
    log_debug("video codec_id:%d\n", video_stream_->codecpar->codec_id);
  }
  // 视频总时长，单位为秒，注意跟音频总时长一样，如果单位是微妙或者毫秒，则这个数值不一定等于视频总时长
  if (video_stream_->duration != AV_NOPTS_VALUE) {
    int duration_video =
        (video_stream_->duration) * av_q2d(video_stream_->time_base);
    // 转换成时分秒格式打印视频时长到控制台
    log_debug("video duration: %02d:%02d:%02d\n", duration_video / 3600,
              (duration_video % 3600) / 60, (duration_video % 60));
  }
}

void Demux::setAudioQueue(std::shared_ptr<PacketQueue> pkt_queue) {
  audio_pkt_queue_ = pkt_queue;
}
void Demux::setVideoQueue(std::shared_ptr<PacketQueue> pkt_queue) {
  video_pkt_queue_ = pkt_queue;
}

void Demux::StartReadPacket() {
  read_packet_thread_ =
      std::make_unique<std::thread>(&Demux::ReadPacketThread, this);
}

void Demux::PushNullPacket(int stream_index,
                           std::shared_ptr<PacketQueue> queue) {
  AVPacket *pkt = av_packet_alloc();
  pkt->data = nullptr;
  pkt->size = 0;
  pkt->stream_index = stream_index;
  queue->Push(pkt);
}
void Demux::ReadPacketThread() {

  int pkt_count = 0;
  int print_max_count = 10;
  log_debug("\n-----av_read_frame start\n");
  int ret = -1;
  while (1) {
    if (player_state_ == PlayerState::PlayerState_Stop) {
      log_info(" PlayerState_Stop exit ReadPacketThread");
      return;
    }
    // 在取出数据的时候做释放
    AVPacket *pkt = av_packet_alloc();
    ret = av_read_frame(format_ctx_, pkt);
    if (ret < 0) {
      // if (audio_stream_index_ >= 0)
      //   PushNullPacket(audio_stream_index_, audio_pkt_queue_);
      // if (video_stream_index_ >= 0)
      //   PushNullPacket(video_stream_index_, video_pkt_queue_);
      log_info("av_read_frame end\n");
      return;
    }
    read_size_ += pkt->size;

    if (pkt->stream_index == audio_stream_index_) {
      // log_debug("Push audio pkt size = %d",pkt->size);
      // dump_file_->WriteBitStream(pkt,audio_stream_->codecpar);
        //log_info("audio_queue size - %d",audio_pkt_queue_->Size());
      audio_pkt_queue_->Push(pkt);

    } else if (pkt->stream_index == video_stream_index_) {
      // log_debug("Push video pkt size = %d",pkt->size);
        // log_info("video_queue size - %d",video_pkt_queue_->Size());
      video_pkt_queue_->Push(pkt);

    }
    write_size_ += pkt->size;
  }
}

Ret Demux::getAudioAvCodecInfo(AVCodecParameters *dec) {
  if (audio_stream_ == nullptr && dec == nullptr) {
    return Ret_ERROR;
  }
  memcpy(dec, audio_stream_->codecpar, sizeof(AVCodecParameters));
  return Ret_OK;
}

Ret Demux::getVideoAvCodecInfo(AVCodecParameters *dec) {
  if (video_stream_ == nullptr && dec == nullptr) {
    return Ret_ERROR;
  }
  memcpy(dec, video_stream_->codecpar, sizeof(AVCodecParameters));
  return Ret_OK;
}

void Demux::set_player_state(PlayerState player_state) {
  log_debug("player state = %d\n", player_state);
  player_state_ = player_state;
}

int Demux::get_audio_stream_index() { return audio_stream_index_; }

int Demux::get_video_stream_index() { return video_stream_index_; }
