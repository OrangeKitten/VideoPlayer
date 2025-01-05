#include "video_frame_provider.h"
#include "video_player.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#undef main //for SDL
int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QGuiApplication app(argc, argv);

    qmlRegisterType<VideoFrameProvider>("VideoFrame.Controls", 1, 0, "VideoFrameProvider");
    qmlRegisterType<VideoPlayer>("VideoPlayer.Controls", 1, 0, "VideoPlayer");

    QQmlApplicationEngine engine;
    const QUrl url(QStringLiteral("qrc:/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
