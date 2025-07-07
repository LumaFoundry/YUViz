import QtQuick.Window 6.0
import QtQuick.Controls 6.0
import Window 1.0

ApplicationWindow {
    title: "videoplayer"
    width: 800
    height: 600
    color: "black"
    visible: true

    property real currentZoom: 1.0
    property real minZoom: 1.0
    property real maxZoom: 1000.0
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

    PinchArea {
        anchors.fill: parent
        pinch.target: videoWindow

        property real initialZoom: 1.0

        onPinchStarted: {
            initialZoom = currentZoom
        }

        onPinchUpdated: {
            let newZoom = initialZoom * pinch.scale;

            currentZoom = Math.max(minZoom, Math.min(newZoom, maxZoom));
            videoWindow.setZoom(currentZoom);
        }

        VideoWindow {
            id: videoWindow
            objectName: "videoWindow"
            anchors.fill: parent

            WheelHandler {
                id: wheelHandler
                acceptedModifiers: Qt.ControlModifier

                property real lastRotation: wheelHandler.rotation

                onRotationChanged: {
                    let delta = wheelHandler.rotation - wheelHandler.lastRotation;

                    if (delta !== 0) {
                        let zoomStep = delta > 0 ? 0.1 : -0.1;
                        let newZoom = currentZoom + zoomStep;

                        currentZoom = Math.max(minZoom, Math.min(newZoom, maxZoom));
                        videoWindow.setZoom(currentZoom);
                    }

                    wheelHandler.lastRotation = wheelHandler.rotation;
                }
            }
        }
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