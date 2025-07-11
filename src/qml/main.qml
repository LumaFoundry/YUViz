import QtQuick.Window 6.0
import QtQuick.Controls 6.0
import QtQuick 6.0
import QtMultimedia 6.5
import Window 1.0

ApplicationWindow {
    id: mainWindow
    title: "videoplayer"
    width: 800
    height: 600
    color: "black"
    visible: true
    flags: Qt.Window
    visibility: Window.Windowed

    property bool isCtrlPressed: false
    property bool isSelecting: false
    property bool wasPlayingBeforeResize: false
    property point selectionStart: Qt.point(0, 0)
    property point selectionEnd: Qt.point(0, 0)
    property bool isProcessingSelection: false
    property bool isMouseDown: false

    Timer {
        id: resizeDebounce
        interval: 100
        repeat: false
        onTriggered: {
            resizing = false
            if (wasPlayingBeforeResize) {
                videoController.play()
            }
        }
    }

    onWidthChanged: {
        if (!resizing) {
            resizing = true
            wasPlayingBeforeResize = videoController.isPlaying()
            if (wasPlayingBeforeResize) {
                videoController.pause()
            }
        }
        resizeDebounce.restart()
    }
    
    onHeightChanged: {
        if (!resizing) {
            resizing = true
            if (wasPlayingBeforeResize) {
                videoController.pause()
            }
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

            if (event.key === Qt.Key_Control) {
                isCtrlPressed = true
                event.accepted = true
            }
        }

        Keys.onReleased: (event) => {
            if (event.key === Qt.Key_Control) {
                isCtrlPressed = false
                event.accepted = true
            }

            if (event.key === Qt.Key_Escape && mainWindow.visibility === Window.FullScreen) {
                mainWindow.visibility = Window.Windowed
                event.accepted = true
            }
        }
    }

    PinchArea {
        anchors.fill: parent
        pinch.target: videoWindow

        property real lastPinchScale: 1.0

        onPinchStarted: {
            lastPinchScale = 1.0
        }

        onPinchUpdated: {
            let factor = pinch.scale / lastPinchScale
            if (factor !== 1.0) {
                videoWindow.zoomAt(factor, pinch.center)
            }
            lastPinchScale = pinch.scale
        }

        VideoWindow {
            id: videoWindow
            objectName: "videoWindow"
            anchors.fill: parent

            maxZoom: 10000.0

            PointHandler {
                id: pointHandler
                acceptedButtons: Qt.LeftButton
                // Enable this handler only when zoomed and NOT doing a selection drag
                enabled: videoWindow.isZoomed && !isCtrlPressed

                property point lastPosition: Qt.point(0, 0)

                onActiveChanged: {
                    isMouseDown = active
                    if (active) {
                        lastPosition = point.position
                    }
                }

                onPointChanged: {
                    if (active) {
                        var currentPosition = point.position
                        var delta = Qt.point(currentPosition.x - lastPosition.x, currentPosition.y - lastPosition.y)

                        if (delta.x !== 0 || delta.y !== 0) {
                            videoWindow.pan(delta)
                        }

                        lastPosition = currentPosition
                    }
                }
            }
            
            WheelHandler {
                id: wheelHandler
                acceptedModifiers: Qt.ControlModifier

                property real lastRotation: wheelHandler.rotation

                onRotationChanged: {
                    let delta = wheelHandler.rotation - wheelHandler.lastRotation;

                    if (delta !== 0) {
                        let factor = delta > 0 ? 1.2 : (1.0 / 1.2);
                        videoWindow.zoomAt(factor, wheelHandler.point.position)
                    }

                    wheelHandler.lastRotation = wheelHandler.rotation;
                }
            }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                hoverEnabled: true // Needed for cursor shape changes
                
                cursorShape: {
                    if (isCtrlPressed) return Qt.CrossCursor
                    if (videoWindow.isZoomed) {
                        return (isMouseDown) ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                    }
                    return Qt.ArrowCursor
                }
                
                onPressed: function(mouse) {
                    if (isCtrlPressed) {
                        // Force reset processing state, ensure new selection can start
                        isProcessingSelection = false
                        isSelecting = true
                        selectionStart = Qt.point(mouse.x, mouse.y)
                        selectionEnd = selectionStart
                    } else {
                        mouse.accepted = false
                    }
                }
                
                onPositionChanged: function(mouse) {
                    if (isSelecting) {
                        var currentPos = Qt.point(mouse.x, mouse.y)
                        var deltaX = currentPos.x - selectionStart.x
                        var deltaY = currentPos.y - selectionStart.y

                        if (deltaX === 0 && deltaY === 0) return

                        selectionEnd = Qt.point(selectionStart.x + deltaX, selectionStart.y + deltaY)
                        selectionCanvas.requestPaint()
                    }
                }
                
                onReleased: function(mouse) {
                    if (isSelecting) {
                        isSelecting = false
                        isProcessingSelection = true
                        
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
                        isProcessingSelection = false
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

    footer: ToolBar {
        Row {
            id: controls
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 10

            Button {
                text: "Play/Pause"
                onClicked: {
                    videoController.togglePlayPause()
                    keyHandler.focus = true //Do not change, Windows requires
                }
            }

            Button {
                text: "Reset View"
                onClicked: {
                    videoWindow.resetZoom()
                    selectionStart = Qt.point(0, 0)
                    selectionEnd = Qt.point(0, 0)
                    isSelecting = false
                    isProcessingSelection = false
                    selectionCanvas.requestPaint()
                    keyHandler.focus = true
                }
            }

            Button {
                text: mainWindow.visibility === Window.FullScreen ? "Exit Fullscreen" : "Enter Fullscreen"
                onClicked: {
                    toggleFullScreen()
                    keyHandler.focus = true
                }
            }
        }
        Row {
            id: playbackControls
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right

            anchors.horizontalCenter: parent.horizontalCenter
            anchors.margins: 20
            spacing: 10

            Slider {
                id: timeSlider
                width: parent.width
                from: 0.0
                to: videoController.duration
                value: dragging ? value : videoController.currentTimeMs
                
                property bool dragging: false

            onPressedChanged: {
                if (pressed) {
                    dragging = true;
                } else {
                    // On release: first seek to current value, then reset dragging state
                    var finalPosition = value;
                    videoController.seekTo(finalPosition);
                    console.log("Slider released, seeking to: " + finalPosition);
                    
                    // Now change dragging state and return focus
                    dragging = false;
                    keyHandler.forceActiveFocus();
                }

                }
            }
        }
    }


    function toggleFullScreen() {
        if (mainWindow.visibility === Window.FullScreen) {
            mainWindow.visibility = Window.Windowed
        } else {
            mainWindow.visibility = Window.FullScreen
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