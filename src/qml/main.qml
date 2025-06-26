import QtQuick 6.0
import QtQuick.Window 6.0
import QtMultimedia 6.5
import Window 1.0

Window {
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
}