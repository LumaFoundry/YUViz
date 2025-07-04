import QtQuick.Window 6.0
import QtQuick.Controls 6.0
import Window 1.0

ApplicationWindow {
    title: "videoplayer"
    width: 800
    height: 600
    color: "black"
    visible: true

    property real scaleFactor: 1.0

    VideoWindow {
        id: videoWindow
        objectName: "videoWindow"
        anchors.fill: parent
    }

    Row {
        id: controls
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.margins: 20
        spacing: 10

        Button {
            text: "Play/Pause"
            onClicked: {
                videoController.togglePlayPause()
            }
        }
    }
}