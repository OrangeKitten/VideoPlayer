// 导入Qt Quick基础模块
import QtQuick 2.2
// 导入窗口管理模块
import QtQuick.Window 2.1
// 导入多媒体模块
import QtMultimedia 5.0
// 导入UI控件模块
import QtQuick.Controls 2.3
// 导入布局模块
import QtQuick.Layouts 1.1
// 导入对话框模块
import QtQuick.Dialogs 1.1

// 导入自定义视频帧控制模块
import VideoFrame.Controls 1.0
// 导入自定义视频播放控制模块
import VideoPlayer.Controls 1.0

// 主应用程序窗口
ApplicationWindow {
    visible: true  // 窗口可见
    width: 1080    // 窗口宽度
    height: 960    // 窗口高度
    title: qsTr("VideoOutput")  // 窗口标题
    property var videocontrol: null  // 视频控制对象
    property bool isPlaying: true    // 播放状态标志
    property var paly_duration:0;
    
    //定时器定义
    Timer {
        id: timer
        interval: 500
        running: false
        repeat: true
        onTriggered: {
            if (videocontrol) {
                //var currentTime = videocontrol.getPlayTime();
               var currentTime = videocontrol.get_current_pts();
                console.log("currentTime = ",currentTime);
                if (currentTime >= 0) {
                    timeDisplay.currentTime = currentTime;
                }
            }
        }
    }

    // 窗口关闭事件处理
    onClosing: function(close) {
         console.log("onClosing");
        if (videocontrol) {
            videocontrol.Stop();      // 停止播放
            videocontrol.destroy();   // 销毁视频控制对象
        }
        close.accepted = true;       // 允许窗口关闭
    }

    // 视频帧提供者组件
    VideoFrameProvider {
        id: provider  // 组件ID
    }
    // 文件选择对话框
    FileDialog {
        id: fileDialog
        title: "Select a file"  // 对话框标题
        folder: shortcuts.movies  // 默认打开电影文件夹
        
        // 文件选择确认事件
        onAccepted: {
            console.log("Selected file: " + fileUrl);
            videocontrol.set_url(fileUrl);  // 设置视频URL
            backpic.visible = false;       // 隐藏背景图片
            videocontrol.Init();           // 初始化视频控制
            videocontrol.setVideoFrameProvider(provider);  // 设置视频帧提供者
            videocontrol.setPlaybackproces(processbar.changePlayBackPro);  // 设置进度回调
            videocontrol.Play();           // 开始播放
            isPlaying = true;              // 更新播放状态
            playPauseBtn.iconSource ="ic_pause.png"  // 更新播放按钮图标
            paly_duration = videocontrol.get_video_duration();
            timeDisplay.totalDuration = paly_duration;
            console.log("paly_duration: " + paly_duration);
            timer.running = true;  // 启动定时器
        }
        
        // 文件选择取消事件
        onRejected: {
            console.log("File selection canceled");
        }
    }

    // 主布局 - 垂直列布局
    Column {
        anchors.fill: parent  // 填充父组件
        spacing: 5           // 子组件间距
        // 视频输出组件
        VideoOutput {
            id: videoout  // 组件ID
            width: parent.width  // 宽度与父组件相同
            height: parent.height - 150  // 高度为父组件高度减去100，为控制栏预留空间
            source: provider  // 视频源
            // 背景图片
            Image {
                id: backpic  // 组件ID
                source: "test.png"  // 图片源
                visible: true       // 默认可见
                z: 1                // Z轴顺序
                fillMode: Image.Stretch  // 图片填充模式
                anchors.fill: parent     // 填充父组件
            }
        }

        // 控制栏容器 - 水平行布局
        Row {
            id: controlContainer  // 组件ID
            width: parent.width * 0.9  // 宽度为父组件的90%
            height: 50                // 固定高度
            spacing: 10               // 子组件间距
            anchors.horizontalCenter: parent.horizontalCenter  // 水平居中
            anchors.top: videoout.bottom  // 位于视频输出组件下方
            anchors.topMargin: 10         // 上边距

            // 进度条和时间显示 - 水平行布局
            Row {
                id: progressRow  // 组件ID
                width: parent.width  // 宽度与父组件相同
                height: 30           // 固定高度
                spacing: 20          // 子组件间距
                anchors.verticalCenter: parent.verticalCenter  // 垂直居中

                // 自定义进度条组件
                ProcessBar {
                    id: processbar  // 组件ID
                    width: parent.width - timeDisplay.width - 20  // 动态宽度
                    height: 20  // 固定高度
                    anchors.verticalCenter: parent.verticalCenter  // 垂直居中
                    anchors.left :parent.left
                    property real maximumValue: 100
                    property bool isDragging: false
                    MouseArea {
                        anchors.fill: parent
                        
                        onPressed: {
                            if (videocontrol ) {
                                 console.log("onPressed");
                                parent.isDragging = true;
                                var pos = mouseX / width * parent.maximumValue;
                                processbar.value = pos;
                                timeDisplay.currentTime = (pos / parent.maximumValue) * timeDisplay.totalDuration;
                            }
                        }
                        onPositionChanged: {
                            //console.log("onPositionChanged");
                            if (parent.isDragging && videocontrol ) {
                                var pos = mouseX / width * parent.maximumValue;
                                processbar.value = pos;
                                timeDisplay.currentTime = (pos / parent.maximumValue) * timeDisplay.totalDuration;
                            }
                        }
                        onReleased: {
                            if (parent.isDragging && videocontrol ) {
                                console.log("onReleased");
                                parent.isDragging = false;
                                var targetTime = (processbar.value / parent.maximumValue) * timeDisplay.totalDuration;
                                console.log("targetTime"+targetTime);
                                videocontrol.StreamSeek(targetTime);
                                timeDisplay.currentTime = targetTime;
                            }
                        }
                    }
                    
                    // 播放进度回调函数
                    
                    function changePlayBackPro(value_) {
                       // console.log("Callback called from QML! value: " + value_ + ", type: " + typeof value_);
                       if(isDragging == false ){
                            if (value_ >= 0) {
                            // 计算当前播放进度
                            var currentprocess = (value_ /timeDisplay.totalDuration ) * maximumValue;
                           // console.log("currentprocess " + currentprocess );
                            // 更新进度条
                            updateProgress(currentprocess);
                            
                        } else {
                            console.error("Invalid progress value: " + value_);
                        }
                       }

                        return "Hello from QML!";
                    }
                }
                // 时间显示文本
                Text {
                    id: timeDisplay  // 组件ID
                    text: "00:00 / 00:00"  // 默认文本
                    color: "red"           // 文本颜色
                    font.pixelSize: 16     // 字体大小
                    anchors.verticalCenter: parent.verticalCenter  // 垂直居中
                    anchors.left : processbar.right
                    
                    property int currentTime: 0  // 当前播放时间
                    property int totalDuration: 0  // 总时长
                    
                    // 时间格式化函数
                    function formatTime(seconds) {
                        let mins = Math.floor(seconds / 60);
                        let secs = Math.floor(seconds % 60);
                        return (mins < 10 ? "0" : "") + mins + ":" + 
                               (secs < 10 ? "0" : "") + secs;
                    }
                    
                    // 更新显示
                    function updateTime() {
                        text = formatTime(currentTime) + " / " + formatTime(totalDuration);
                        processbar.changePlayBackPro(currentTime);
                    }
                    
                    // 绑定属性变化
                    onCurrentTimeChanged: updateTime()
                    onTotalDurationChanged: updateTime()
                }
            }
        }



        // 按钮控制栏 - 水平行布局
        Row {
            id: buttoncontroller  // 组件ID
            width: parent.width   // 宽度与父组件相同
            height: 70            // 固定高度
            spacing: 5            // 子组件间距
            anchors {
                top: controlContainer.bottom  // 位于控制栏下方
                topMargin: 10                 // 上边距
                horizontalCenter: parent.horizontalCenter  // 水平居中
            }
            layoutDirection: Qt.LeftToRight  // 布局方向从左到右

            // 打开文件按钮
            FlatButton {
                text: "Open File"  // 按钮文本
                iconSource: "打开文件夹.png"  // 按钮图标
                padding: 10        // 内边距
                iconSize: Qt.size(32, 32)  // 图标尺寸
                ToolTip.visible: hovered  // 鼠标悬停显示提示
                ToolTip.delay: 1000       // 提示延迟
                ToolTip.text: "打开视频文件"  // 提示文本
                
                // 点击事件处理
                onClicked: {
                    if (!videocontrol) {
                        try {
                            // 动态创建视频控制对象
                            videocontrol = Qt.createQmlObject('
                                import VideoPlayer.Controls 1.0;
                                VideoPlayer {
                                    id: videocontrol;
                                }', parent, "dynamicvideocontrol");
                        } catch (error) {
                            console.error("Failed to create video control:", error);
                        }
                    }
                    fileDialog.open();  // 打开文件选择对话框
                }
            }

            // 播放/暂停按钮
            FlatButton {
                id: playPauseBtn  // 组件ID
                text: isPlaying ? "play":"pause"  // 动态文本
                iconSource: isPlaying ? "ic_play.png": "ic_pause.png"  // 动态图标
                ToolTip.visible: hovered  // 鼠标悬停显示提示
                ToolTip.delay: 1000       // 提示延迟
                ToolTip.text: isPlaying ?"播放视频":  "暂停播放"  // 动态提示文本
                
                // 点击事件处理
                onClicked: {
                    if (isPlaying) {
                        videocontrol.Pause();  // 暂停播放
                        isPlaying = false;     // 更新状态
                        timer.running = false;  // 停止定时器
                    } else {
                        backpic.visible = false;  // 隐藏背景图片
                        videocontrol.Pause();     // 继续播放
                        isPlaying = true;         // 更新状态
                        timer.running = true;  // 启动定时器
                    }
                }
            }

            // 停止按钮
            FlatButton {
                id: stop  // 组件ID
                text: "stop"  // 按钮文本
                iconSource: "ic_stop.png"  // 按钮图标
                ToolTip.visible: hovered  // 鼠标悬停显示提示
                ToolTip.delay: 1000       // 提示延迟
                ToolTip.text: "停止播放"  // 提示文本
                
                // 点击事件处理
                onClicked: {
                    videocontrol.Stop();      // 停止播放
                    videocontrol.destroy();   // 销毁视频控制对象
                    backpic.visible = true;   // 显示背景图片
                    timer.running = false;  // 停止定时器
                }
            }
        }
    }
}
