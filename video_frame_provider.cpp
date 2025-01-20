#include "video_frame_provider.h"

#include <QFile>
#include <QFileInfo>
#include <QSharedPointer>
#include <QTimer>

class VideoFrameProviderPrivate
{
public:
    QAbstractVideoSurface *m_surface = nullptr;
    QVideoSurfaceFormat m_format;
    QString m_videoUrl;
    QSharedPointer<QVideoFrame> m_frame = nullptr;

    /**
     * 以下代码仅供示例使用
     * 实际上，此处应放你的视频源，它可以来自你自己的解码器
     */
    QVector<char> m_testData;
    QTimer *m_testTimer = nullptr;

};

VideoFrameProvider::VideoFrameProvider(QObject *parent)
    : QObject(parent)
{
    d = new VideoFrameProviderPrivate;
     dump_file_ = std::make_unique<FileDump>("video_frame.yuv");
    int width = 1920;
    int height = 800;
    int size = width * height * 3 / 2;// YUV420P

    setFormat(width, height, QVideoFrame::Format_YUV420P);

    d->m_frame.reset(new QVideoFrame(size, QSize(width, height), width, QVideoFrame::Format_YUV420P));
    d->m_testTimer = new QTimer(this);

    //读入一张yuv图像
//    QFile file("./test.yuv");
//    file.open(QIODevice::ReadOnly);
//    QByteArray data = file.readAll();
//    d->m_testData.resize(data.size());
//    memcpy(d->m_testData.data(), data.constData(), size_t(data.size()));
//    file.close();

//    connect(d->m_testTimer, &QTimer::timeout, this, [=]{
//        static int count = 0;

//        //简单变化一下，模拟视频帧
//        if (++count & 0x1) {
//            for (auto &it : d->m_testData) {
//                it *= 0.5;
//            }
//        } else {
//            for (auto &it : d->m_testData) {
//                it *= 2.0;
//            }
//        }

//        if (d->m_frame->map(QAbstractVideoBuffer::WriteOnly)) {
//            memcpy(d->m_frame->bits(), d->m_testData.data(), size_t(d->m_testData.size()));
//            d->m_frame->unmap();
//            emit newVideoFrame(*(d->m_frame.data()));

//        };
//    });
    connect(this, &VideoFrameProvider::newVideoFrame, this, &VideoFrameProvider::onNewVideoFrameReceived);

    //d->m_testTimer->start(200);
}

void VideoFrameProvider::VideoRender(const AVFrame *frame) {

  // dump_file_->WriteVideoYUV420PData(frame);
   if (d->m_frame->map(QAbstractVideoBuffer::WriteOnly)) {
       // 假设使用的是 YUV420P 格式
       uint8_t *bits = d->m_frame->bits();

       // Y 平面
       memcpy(bits, frame->data[0], frame->linesize[0] * frame->height);

       // U 平面
       memcpy(bits + frame->linesize[0] * frame->height, frame->data[1], frame->linesize[1] * frame->height / 2);

       // V 平面
       memcpy(bits + frame->linesize[0] * frame->height + frame->linesize[1] * frame->height / 2, frame->data[2], frame->linesize[2] * frame->height / 2);

       d->m_frame->unmap();
       emit newVideoFrame(*(d->m_frame.data()));
   }
}
 //这种写法864 x 368 可以播放 但是三个for循环卡顿
// void VideoFrameProvider::VideoRender(const AVFrame *frame) {
//     dump_file_->WriteVideoYUV420PData(frame);
//     if (d->m_frame->map(QAbstractVideoBuffer::WriteOnly)) {
//         // 假设使用的是 YUV420P 格式
//         uint8_t *bits = d->m_frame->bits();
//         int width = d->m_frame->width();
//         int height = d->m_frame->height();

//         // 计算各平面大小
//         int y_size = width * height;
//         int uv_size = y_size / 4;

//         // Y 平面
//         for (int i = 0; i < height; i++) {
//             memcpy(bits + width * i,
//                   frame->data[0] + frame->linesize[0] * i,
//                   width);
//         }

//         // U 平面
//         for (int i = 0; i < height/2; i++) {
//             memcpy(bits + y_size + width/2 * i,
//                   frame->data[1] + frame->linesize[1] * i,
//                   width/2);
//         }

//         // V 平面
//         for (int i =  0; i < height/2; i++) {
//             memcpy(bits + y_size + uv_size + width/2 * i,
//                   frame->data[2] + frame->linesize[2] * i,
//                   width/2);
//         }

//         d->m_frame->unmap();
//         emit newVideoFrame(*(d->m_frame.data()));
//     }
// }

VideoFrameProvider::~VideoFrameProvider()
{
    if (d) delete d;
}

QAbstractVideoSurface *VideoFrameProvider::videoSurface()
{
    return d->m_surface;
}

void VideoFrameProvider::setVideoSurface(QAbstractVideoSurface *surface)
{
    if (d->m_surface && d->m_surface != surface && d->m_surface->isActive()) {
        d->m_surface->stop();
    }

    d->m_surface = surface;

    if (d->m_surface && d->m_format.isValid()) {
        d->m_format = d->m_surface->nearestFormat(d->m_format);
        d->m_surface->start(d->m_format);
    }
}

QString VideoFrameProvider::videoUrl() const
{
    return d->m_videoUrl;
}

void VideoFrameProvider::setVideoUrl(const QString &url)
{
    if (d->m_videoUrl != url) {
        d->m_videoUrl = url;
        emit videoUrlChanged();
    }
}

void VideoFrameProvider::setFormat(int width, int heigth, QVideoFrame::PixelFormat pixFormat)
{
    QVideoSurfaceFormat format(QSize(width, heigth), pixFormat);
    d->m_format = format;

    if (d->m_surface) {
        if (d->m_surface->isActive()) {
            d->m_surface->stop();
        }
        d->m_format = d->m_surface->nearestFormat(format);
        d->m_surface->start(d->m_format);
    }
}

void VideoFrameProvider::onNewVideoFrameReceived(const QVideoFrame &frame)
{
    if (d->m_surface)
        d->m_surface->present(frame);
}


