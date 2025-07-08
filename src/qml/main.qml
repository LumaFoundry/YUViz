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
    property bool resizing: false

    Timer {
        id: resizeDebounce
        interval: 100
        repeat: false
        onTriggered: {
            resizing = false
            videoController.play()
        }
    }

    onWidthChanged: {
        if (!resizing) {
            resizing = true
            videoController.pause()
        }
        resizeDebounce.restart()
    }
    onHeightChanged: {
        if (!resizing) {
            resizing = true
            videoController.pause()
        }
        resizeDebounce.restart()
    }

    // Top-level key listener
    Item {
        id: keyHandler
        anchors.fill: parent
        focus: true
        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Space) {
                console.log("Space key pressed");
                videoController.togglePlayPause()
                event.accepted = true
            }

            if (event.key == Qt.Key_Right) {
                console.log("Right arrow key pressed");
                videoController.stepForward()
                event.accepted = true
            }

            if (event.key == Qt.Key_Left) {
                console.log("Left arrow key pressed");
                videoController.stepBackward()
                event.accepted = true
            }
        }
    }

    VideoWindow {
        id: videoWindow
        objectName: "videoWindow"
        anchors.fill: parent
    }
}