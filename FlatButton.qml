import QtQuick 2.2

Rectangle {
    id: bkgnd;
    implicitWidth: 60;
    implicitHeight: 88;
    color: "transparent";
    property alias iconSource: icon.source;
    property alias iconWidth: icon.width;
    property alias iconHeight: icon.height;
    property alias textColor: btnText.color;
    property alias font: btnText.font;
    property alias text: btnText.text;
    radius: 6;
    property bool hovered: false;
    border.color: "#949494";
    border.width: hovered ? 2 : 0;
    signal clicked;

    property int padding: 10
    property size iconSize: Qt.size(32, 32)

    Column {
        id: content
        anchors.centerIn: parent
        spacing: 5
        // padding: parent.padding

        Image {
            id: icon
            width: iconSize.width
            height: iconSize.height
            fillMode: Image.PreserveAspectFit
            sourceSize: iconSize
        }

        Text {
            id: btnText
            color: ma.pressed ? "blue" : (parent.hovered ? "#0000a0" : "white")
            font.pixelSize: 14
            horizontalAlignment: Text.AlignHCenter
        }
    }
    MouseArea {
        id: ma;
        anchors.fill: parent;
        hoverEnabled: true;
        onEntered: {
            bkgnd.hovered = true;
        }
        onExited: {
            bkgnd.hovered = false;
        }
        onClicked: {
            if(Qt.platform.os == "android"){
                bkgnd.hovered = false;
            }
            bkgnd.clicked();
        }
    }
}
