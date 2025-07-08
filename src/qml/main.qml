import QtQuick.Window 6.0
import QtQuick.Controls 6.0
import QtQuick 6.0
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

    property real scaleFactor: 1.0
    property bool isCtrlPressed: false
    property bool isSelecting: false
    property point selectionStart: Qt.point(0, 0)
    property point selectionEnd: Qt.point(0, 0)
    property bool isProcessingSelection: false

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

        Item {
            anchors.fill: parent
            focus: true
            
            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_Control) {
                    isCtrlPressed = true
                }
            }
            
            Keys.onReleased: function(event) {
                if (event.key === Qt.Key_Control) {
                    isCtrlPressed = false
                }
            }
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

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                
                onPressed: function(mouse) {
                    if (isCtrlPressed) {
                        // Force reset processing state, ensure new selection can start
                        isProcessingSelection = false
                        isSelecting = true
                        selectionStart = Qt.point(mouse.x, mouse.y)
                        selectionEnd = selectionStart
                    }
                }
                
                onPositionChanged: function(mouse) {
                    if (isSelecting) {
                        selectionEnd = Qt.point(mouse.x, mouse.y)
                        selectionCanvas.requestPaint()
                    }
                }
                
                onReleased: function(mouse) {
                    if (isSelecting) {
                        isSelecting = false
                        isProcessingSelection = true
                        
                        selectionEnd = Qt.point(mouse.x, mouse.y)
                        
                        // Calculate rectangle area
                        var rect = Qt.rect(
                            Math.min(selectionStart.x, selectionEnd.x),
                            Math.min(selectionStart.y, selectionEnd.y),
                            Math.abs(selectionEnd.x - selectionStart.x),
                            Math.abs(selectionEnd.y - selectionStart.y)
                        )
                        
                        console.log("Final selection rect:", rect.x, rect.y, rect.width, rect.height)
                              
                        videoWindow.setSelectionRect(rect)
                        videoWindow.zoomToSelection()
                        
                        // Clear selection state, make rectangle disappear
                        selectionStart = Qt.point(0, 0)
                        selectionEnd = Qt.point(0, 0)
                        selectionCanvas.requestPaint()
                        
                    }
                }
            }
        
        // Draw selection rectangle
            Canvas {
                id: selectionCanvas
                anchors.fill: parent
                z: 1
                
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    if (isSelecting || (selectionStart.x !== 0 || selectionStart.y !== 0)) {
                        var rect = Qt.rect(
                            Math.min(selectionStart.x, selectionEnd.x),
                            Math.min(selectionStart.y, selectionEnd.y),
                            Math.abs(selectionEnd.x - selectionStart.x),
                            Math.abs(selectionEnd.y - selectionStart.y)
                        )
                        ctx.fillStyle = "rgba(0, 0, 255, 0.2)"
                        ctx.fillRect(rect.x, rect.y, rect.width, rect.height)
                        ctx.strokeStyle = "blue"
                        ctx.lineWidth = 2
                        ctx.strokeRect(rect.x, rect.y, rect.width, rect.height)
                    }
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

        Button {
            text: "Reset View"
            onClicked: {
                videoWindow.resetZoom()
                zoomSlider.value = 1.0
                selectionStart = Qt.point(0, 0)
                selectionEnd = Qt.point(0, 0)
                isSelecting = false
                isProcessingSelection = false
                selectionCanvas.requestPaint()
            }
        }
    }
    Text {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 10
        color: "white"
        text: isCtrlPressed ? "Hold Ctrl key and drag mouse to draw rectangle selection area" : "Press Ctrl key to start selection area"
        font.pixelSize: 14
    }
}