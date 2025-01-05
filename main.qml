import QtQuick 2.2
import QtQuick.Window 2.1
import QtMultimedia 5.0
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1
import QtQuick.Dialogs 1.1

import VideoFrame.Controls 1.0
import VideoPlayer.Controls 1.0

Window {
    visible: true
    width: 1080
    height: 960
    title: qsTr("VideoOutput")
    property var videocontrol: null
    VideoFrameProvider {
        id: provider
        // videoUrl: "rtsp://xxx.xxx.xxx/channel=1"
    }

    // VideoPlayer {
    //     id: videocontrol
    //     // videoUrl: "rtsp://xxx.xxx.xxx/channel=1"
    //    Component.onCompleted: {
    //        console.log("VideoPlayer");

    //    }
    // }

    VideoOutput {
        width: 640 // 设置宽度为 640
        height: 480 // 设置高度为 480
        anchors.fill: parent
        source: provider
    }

    FileDialog {
        id: fileDialog
        title: "Select a file"
        folder: shortcuts.movies // 默认打开的文件夹
        onAccepted: {
            console.log("Selected file: " + fileUrl)
           videocontrol.set_url(fileUrl);
            //selectedFileUrl.text = fileUrl // 显示选中文件的 URL
        }
        onRejected: {
            console.log("File selection canceled")
        }
    }

    

    Row {
        id: controller
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 4
        spacing: 4

        FlatButton {
                text: "Open File"
                iconSource: "打开文件夹.png"
                onClicked: {
                    if (!videocontrol) {
                        // 动态创建 VideoPlayer 对象
                        try {
                            videocontrol = Qt.createQmlObject('
                                import VideoPlayer.Controls 1.0;
                                VideoPlayer {
                                    id: videocontrol;
                                }', parent, "dynamicvideocontrol");
                            console.log("VideoPlayer created successfully");
                        } catch (error) {
                            console.error("Error creating VideoPlayer:", error);
                        }
                    }
                    fileDialog.open();  // 打开文件对话框
                }
            }

        FlatButton {
            id: start
            text: "play"
            iconSource: "ic_play.png"
            onClicked: {
                videocontrol.Init();
                videocontrol.setVideoFrameProvider(provider)
                videocontrol.Play()
            }
        }
        FlatButton {
            id: pause
            text: "pause"
            iconSource: "ic_pause.png"
            onClicked: {
                videocontrol.Pause()
            }
        }
        FlatButton {
            id: stop
            text: "stop"
            iconSource: "ic_stop.png"
            onClicked: {
                videocontrol.Stop();
                videocontrol.destroy(); // Destroys the VideoPlayer object
            }
        }
//            FlatButton {
//                id: destroy
//                text: "destroy"
//                iconSource: "ic_stop.png"
//                onClicked: {
//                    videocontrol.destroy(); // Destroys the VideoPlayer object
//                }
//            }
        }
    }

