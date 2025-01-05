
#ifndef __VIDEO_PLAYER_H__
#define __VIDEO_PLAYER_H__

#include"demux.h"
#include <memory>
#include "type.h"
#include "decodec.h"
#include <mutex>
#include <QObject>
#include "video_frame_provider.h"
 using std::placeholders::_1;
class VideoPlayer :public QObject{
    Q_OBJECT
public:
    VideoPlayer(QObject *parent = nullptr);
    ~VideoPlayer();
    Q_INVOKABLE void Init();
    Q_INVOKABLE void Play();
    Q_INVOKABLE void Pause();
    Q_INVOKABLE void Stop();
    Q_INVOKABLE void set_url(const QString &url);

     void set_video_frame_render_callback(const WriteYUVDataCallback &cb) ;
     Q_INVOKABLE void setVideoFrameProvider(VideoFrameProvider *provider) ;

//    void SetPause(bool pause);
//    bool GetPause();
//    void SetPlay(bool play);
//    bool GetPlay();
//    void SetPosition(int position);
//    int GetPosition();
   void set_volume(int volume);
   int get_volume();
   Ret set_mute(bool mute);
   PlayerState getState();

private:
    std::unique_ptr<Demux> demux_;
    std::unique_ptr<Decodec> decodec_;
    std::shared_ptr<PacketQueue>audio_pkt_queue_;
    std::shared_ptr<PacketQueue>video_pkt_queue_;
    PlayerState player_state_;
    bool pause_state_;
    bool last_pause_state_;
    int position_;
    int volume_;
    std ::string url_;
    std::mutex mtx_;
    VideoFrameProvider *videoFrameProvider_ = nullptr;
};
#endif
