#ifndef VIDEOFRAMEPROVIDER_H
#define VIDEOFRAMEPROVIDER_H

#include <QObject>
#include <QAbstractVideoSurface>
#include <QVideoSurfaceFormat>
#include <memory>
#include "file_dump.h"
extern "C"
{
#include "libavcodec/avcodec.h"
}
QT_FORWARD_DECLARE_CLASS(VideoFrameProviderPrivate);

class VideoFrameProvider : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QAbstractVideoSurface *videoSurface READ videoSurface WRITE setVideoSurface)
    Q_PROPERTY(QString videoUrl READ videoUrl WRITE setVideoUrl NOTIFY videoUrlChanged)

public:
    VideoFrameProvider(QObject *parent = nullptr);
    ~VideoFrameProvider();

    QAbstractVideoSurface *videoSurface();
    void setVideoSurface(QAbstractVideoSurface *surface);

    QString videoUrl() const;
    void setVideoUrl(const QString &url);
    void setFormat(int width, int heigth, QVideoFrame::PixelFormat pixFormat);

     void VideoRender(const AVFrame *frame);

signals:
    /**
     * @brief newVideoFrame 有新的视频帧
     * @param frame 视频帧
     */
    void newVideoFrame(const QVideoFrame &frame);
    void videoUrlChanged();

public slots:
    void onNewVideoFrameReceived(const QVideoFrame &frame);

private:
    VideoFrameProviderPrivate *d = nullptr;
    std::unique_ptr<FileDump> dump_file_;
};


#endif // VIDEOFRAMEPROVIDER_H
