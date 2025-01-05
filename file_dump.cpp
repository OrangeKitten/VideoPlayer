#include "file_dump.h"
extern "C" {
#include "log.h"
}
Auido_Info audio_info_ = {};
void setAudioInfo(const Auido_Info &audio_info) { audio_info_ = audio_info; }
const int sampling_frequencies[] = {
    96000, // 0x0
    88200, // 0x1
    64000, // 0x2
    48000, // 0x3
    44100, // 0x4
    32000, // 0x5
    24000, // 0x6
    22050, // 0x7
    16000, // 0x8
    12000, // 0x9
    11025, // 0xa
    8000   // 0xb
           // 0xc d e f是保留的
};
FileDump::FileDump(const char *filename) {

  file_ = fopen(filename, "wb+");
  if (!file_) {
    log_error("Error opening file");
  }
}
FileDump::~FileDump() {
  if (file_) {
    fclose(file_);
  }
}
void FileDump::WriteBitStream(const AVPacket *data, int size,AVCodecID format) {
  AVPacket *pkt = (AVPacket *)data;
#if 1
    //在容器格式是mp4的时候，dump aac数据需要手动加包头
    if(format == AV_CODEC_ID_AAC) {
          char adts_header_buf[7] = {0};
            adts_header(adts_header_buf, pkt->size,
                        1,
                        44100,
                        2);
            //printf("codecparID = %d\n",ifmt_ctx->streams[audio_index]->codecpar->codec_type);
            fwrite(adts_header_buf, 1, 7, file_);  // 写adts header , ts流不适用，ts流分离出来的packet带了adts header
            int len = fwrite( pkt->data, 1, pkt->size, file_);   // 写adts data
            if(len != pkt->size)
            {
                log_error(NULL, AV_LOG_DEBUG, "warning, length of writed data isn't equal pkt.size(%d, %d)\n",
                       len,
                       pkt->size);
            }

    }
#else 0
  int len = fwrite(pkt->data, 1, pkt->size, file_); // 写adts data
  if (len != pkt->size) {
    log_error(NULL, AV_LOG_DEBUG,
              "warning, length of writed data isn't equal pkt.size(%d, %d)\n",
              len, pkt->size);
  }
  #endif
}

 void FileDump::WriteBitStream(const AVPacket *data, AVCodecParameters*para) {
    AVPacket *pkt = (AVPacket *)data;
#if 1
    //在容器格式是mp4的时候，dump aac数据需要手动加包头
    if(para->codec_id == AV_CODEC_ID_AAC) {
          char adts_header_buf[7] = {0};
            adts_header(adts_header_buf, pkt->size,
                        para->profile,
                        para->sample_rate,
                        para->channels);
            //printf("codecparID = %d\n",ifmt_ctx->streams[audio_index]->codecpar->codec_type);
            fwrite(adts_header_buf, 1, 7, file_);  // 写adts header , ts流不适用，ts流分离出来的packet带了adts header
            int len = fwrite( pkt->data, 1, pkt->size, file_);   // 写adts data
            if(len != pkt->size)
            {
                log_error(NULL, AV_LOG_DEBUG, "warning, length of writed data isn't equal pkt.size(%d, %d)\n",
                       len,
                       pkt->size);
            }

    }
#else 0
  int len = fwrite(pkt->data, 1, pkt->size, file_); // 写adts data
  if (len != pkt->size) {
    log_error(NULL, AV_LOG_DEBUG,
              "warning, length of writed data isn't equal pkt.size(%d, %d)\n",
              len, pkt->size);
  }
  #endif

 }

void FileDump::WritePcmPlanarData(uint8_t *data[], int size_per_sample) {
  /**
      P表示Planar（平面），其数据格式排列方式为 :
      LLLLLLRRRRRRLLLLLLRRRRRRLLLLLLRRRRRRL...（每个LLLLLLRRRRRR为一个音频帧）
   注意planar格式通常是ffmpeg内部使用格式
      而不带P的数据格式（即交错排列）排列方式为：
      LRLRLRLRLRLRLRLRLRLRLRLRLRLRLRLRLRLRL...（每个LR为一个音频样本）
   播放范例：   `
    */
// 解析出来的是32bit float planar 小端数据
    for (int i = 0; i < audio_info_.samples; i++) {
      for (int ch = 0; ch < audio_info_.channels;
           ch++) // 交错的方式写入, 大部分float的格式输出
        fwrite(data[ch] + size_per_sample * i, 1, size_per_sample, file_);
      fflush(file_);
    }
  
}

void FileDump::WritePcmData(uint8_t *data[], int pcm_size) {

    fwrite(data[0], 1, pcm_size, file_);
    fflush(file_);

}

void FileDump::WritePcmData(uint8_t *data, int pcm_size) {
      fwrite(data, 1, pcm_size, file_);
    fflush(file_);
}

int FileDump::adts_header(char *const p_adts_header, const int data_length,
                          const int profile, const int samplerate,
                          const int channels) {

  int sampling_frequency_index = 3; // 默认使用48000hz
  int adtsLen = data_length + 7;

  int frequencies_size =
      sizeof(sampling_frequencies) / sizeof(sampling_frequencies[0]);
  int i = 0;
  for (i = 0; i < frequencies_size; i++) {
    if (sampling_frequencies[i] == samplerate) {
      sampling_frequency_index = i;
      break;
    }
  }
  if (i >= frequencies_size) {
    printf("unsupport samplerate:%d\n", samplerate);
    return -1;
  }

  p_adts_header[0] = 0xff; // syncword:0xfff                          高8bits
  p_adts_header[1] = 0xf0; // syncword:0xfff                          低4bits
  p_adts_header[1] |= (0 << 3); // MPEG Version:0 for MPEG-4,1 for MPEG-2  1bit
  p_adts_header[1] |= (0 << 1); // Layer:0                                 2bits
  p_adts_header[1] |= 1;        // protection absent:1                     1bit

  p_adts_header[2] = (profile) << 6; // profile:profile               2bits
  p_adts_header[2] |=
      (sampling_frequency_index & 0x0f)
      << 2; // sampling frequency index:sampling_frequency_index  4bits
  p_adts_header[2] |= (0 << 1); // private bit:0                   1bit
  p_adts_header[2] |=
      (channels & 0x04) >> 2; // channel configuration:channels  高1bit

  p_adts_header[3] = (channels & 0x03)
                     << 6;      // channel configuration:channels 低2bits
  p_adts_header[3] |= (0 << 5); // original：0                1bit
  p_adts_header[3] |= (0 << 4); // home：0                    1bit
  p_adts_header[3] |= (0 << 3); // copyright id bit：0        1bit
  p_adts_header[3] |= (0 << 2); // copyright id start：0      1bit
  p_adts_header[3] |= ((adtsLen & 0x1800) >> 11); // frame length：value 高2bits

  p_adts_header[4] =
      (uint8_t)((adtsLen & 0x7f8) >> 3); // frame length:value    中间8bits
  p_adts_header[5] =
      (uint8_t)((adtsLen & 0x7) << 5); // frame length:value    低3bits
  p_adts_header[5] |= 0x1f;            // buffer fullness:0x7ff 高5bits
  p_adts_header[6] =
      0xfc; // ‭11111100‬       //buffer fullness:0x7ff 低6bits
  // number_of_raw_data_blocks_in_frame：
  //    表示ADTS帧中有number_of_raw_data_blocks_in_frame + 1个AAC原始帧。

  return 0;
}

bool FileDump::is_audio_format_planar() {
  // 检查每个声道的指针是否不同
  if (audio_info_.format == AV_SAMPLE_FMT_U8P ||
      audio_info_.format == AV_SAMPLE_FMT_S16P ||
      audio_info_.format == AV_SAMPLE_FMT_S32P ||
      audio_info_.format == AV_SAMPLE_FMT_FLTP ||
      audio_info_.format == AV_SAMPLE_FMT_DBLP ||
      audio_info_.format == AV_SAMPLE_FMT_S64P) {
    return true;
  }
  return false;
}

void FileDump::WriteVideoYUV420PData(const AVFrame* videoFrame) {
    WriteVideoYUV420PData(const_cast<AVFrame*>(videoFrame));
}

void FileDump::WriteVideoYUV420PData(AVFrame* videoFrame) {
    for (int j = 0; j < videoFrame->height; j++)
        fwrite(videoFrame->data[0] + j * videoFrame->linesize[0], 1, videoFrame->width, file_);
    for (int j = 0; j < videoFrame->height / 2; j++)
        fwrite(videoFrame->data[1] + j * videoFrame->linesize[1], 1, videoFrame->width / 2, file_);
    for (int j = 0; j < videoFrame->height / 2; j++)
        fwrite(videoFrame->data[2] + j * videoFrame->linesize[2], 1, videoFrame->width / 2, file_);
}