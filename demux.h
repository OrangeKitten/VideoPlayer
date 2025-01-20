
#ifndef __DEMUX_H__
#define __DEMUX_H__
#include <string>
#include "packet_deque.h"
#include <memory>
#include <thread>
#include "type.h"
#include "file_dump.h"

extern "C"
{
#include <libavformat/avformat.h>
}
class Demux
{
public:
    Demux(const char *url);
    ~Demux();
    void setAudioQueue(std::shared_ptr<PacketQueue>pkt_queue);
    void setVideoQueue(std::shared_ptr<PacketQueue>pkt_queue);
    void StartReadPacket();
    Ret getAudioAvCodecInfo( AVCodecParameters *dec);
    Ret getVideoAvCodecInfo( AVCodecParameters *dec);
    AVRational getVideoTimeBase();
    AVRational getAudioTimeBase();
    AVRational getVideoFrameRate();
    void set_player_state(PlayerState player_state);
    void PushNullPacket(int stream_index,std::shared_ptr<PacketQueue>queue);
    int get_video_stream_index();
    int get_audio_stream_index();
    int get_video_duration();
    AUDIO_INFO get_audio_info();
    void StreamSeek(int64_t pos);
   
private:
    void DumpMedioInfo();
    int OpenFile();
    void ReadPacketThread();


public:
AVRational frame_rate_;
AUDIO_INFO audio_info_;
private:
    AVFormatContext *format_ctx_;
    AVIOContext *avio_ctx_;
    AVStream *video_stream_;
    AVStream *audio_stream_;
    int video_stream_index_;
    int audio_stream_index_;
    const char *url_;
    int pkt_count_;
    std::shared_ptr<PacketQueue>audio_pkt_queue_;
    std::shared_ptr<PacketQueue>video_pkt_queue_;
    std::unique_ptr<std::thread> read_packet_thread_;
    int read_size_;
    int write_size_;
    std::unique_ptr<FileDump>  dump_file_;
    PlayerState player_state_;
    bool seek_ = false;
    int64_t seek_pos_ = 0; //秒
    int64_t seek_rel_ = 0;

    
    

};
#endif // DEMUX_H
