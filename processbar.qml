import QtQuick 2.2
import QtQuick.Controls 1.2
import QtQuick.Controls.Styles 1.2
import QtMultimedia 5.8

// 用于处理 GIF 动画
Rectangle {
    width: 320
    height: 240
    color: "transparent"
    property alias palyprocess: playerProcessBar.value;
    property real value: playerProcessBar.value

    onValueChanged: {
       // console.log("ProcessBar value changed to:", value);
        playerProcessBar.value = value;
    }

    function updateProgress(value) {
        console.log("Updating progress to:", value);
        this.value = value;
    }

    Slider {
        id: playerProcessBar
        width: parent.width
        height: parent.height
        anchors.bottom: parent.bottom
        minimumValue: 0
        maximumValue: 100
        stepSize: 1
        value: 0
        tickmarksEnabled: false // 不显示刻度
        style: SliderStyle {
            // 背景槽
            groove: Item {
                implicitWidth: 200
                implicitHeight: 8
                Rectangle {
                    id: grooveBackground
                    anchors.fill: parent
                    color: "black"
                    radius: 8
                }
                // 高亮部分
                Rectangle {
                    id: highlight
                    height: parent.height
                    width: playerProcessBar.value / playerProcessBar.maximumValue
                           * playerProcessBar.width
                    color: "green" // 高亮颜色
                    radius: 8
                }
            }
            handle: Rectangle {
                // 滑块
                implicitWidth: 6 // 滑块宽度
                anchors.centerIn: parent
                color: control.pressed ? "white" : "lightgray"
                border.color: "gray"
                border.width: 2
                width: 20
                height: 20
                radius: 12
                // Text {
                //     anchors.centerIn: parent
                //     text: control.value
                //     color: "red"
                // }
                // AnimatedImage {
                //     source: "huli.gif"
                //     anchors.fill: parent
                //     //playing: control.pressed || control.hovered
                //     playing:auto;
                // }
            }
}
}
}
