import QtQuick 6.0
import QtQuick.Controls.Basic 6.0
import Window 1.0
import QtQuick.Layouts 1.15

VideoWindow {
    id: videoWindow
    property int videoId: -1
    property bool assigned: false
    property bool isSelecting: false
    property point selectionStart: Qt.point(0, 0)
    property point selectionEnd: Qt.point(0, 0)
    property bool isProcessingSelection: false
    property bool isMouseDown: false
    property bool isCtrlPressed: keyHandler.isCtrlPressed
    property bool isZoomed: sharedView ? sharedView.isZoomed : false
    objectName: "videoWindow_" + videoId
    signal requestRemove(int videoId)

    QtObject {
        id: videoBridge
        objectName: "videoBridge"

        function setColorSpaceIndex(index) {
            colorSpaceSelector.currentIndex = index;
            return 0;
        }
    }


    Row {
        anchors.top: parent.top
        anchors.right: parent.right
        spacing: 8

        ComboBox {
            width: 0
            height: 40
            model: ["Don't remove this ComboBox"]
            indicator: Canvas {}
        }
        
        Button {
            id: menuButton
            width: 40
            height: 40
            text: "⚙"
            font.pixelSize: 40
            background: Rectangle {
                color: "transparent"
            }
            contentItem: Text {
                text: menuButton.text
                font.pixelSize: menuButton.font.pixelSize
                color: "white"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: menu.open()

            Menu {
                id: menu
                y: menuButton.height

                MenuItem {
                    contentItem: ComboBox {
                        id: colorSpaceSelector
                        objectName: "colorSpaceSelector"
                        model: ["BT709", "BT709 Full", "BT470BG", "BT470BG Full", "BT2020", "BT2020 Full"]
                        width: 160
                        currentIndex: 0

                        onCurrentIndexChanged: {
                            let colorSpaceMap = [
                                { space: 1, range: 1 },  // BT709 MPEG
                                { space: 1, range: 2 },  // BT709 Full
                                { space: 5, range: 1 },  // BT470BG MPEG
                                { space: 5, range: 2 },  // BT470BG Full
                                { space: 10, range: 1 }, // BT2020_CL MPEG
                                { space: 10, range: 2 }  // BT2020_CL Full
                            ];
                            let selected = colorSpaceMap[currentIndex];
                            videoWindow.setColorParams(selected.space, selected.range);
                            keyHandler.forceActiveFocus();
                        }

                        onActivated: keyHandler.forceActiveFocus()
                    }
                }

                MenuSeparator {}

                MenuItem {
                    text: "Close the Video"
                    onTriggered: videoWindow.requestRemove(videoId)
                }
            }
        }
    }

    PinchArea {
        anchors.fill: parent
        pinch.target: videoWindow

        property real lastPinchScale: 1.0

        onPinchStarted: {
            lastPinchScale = 1.0;
        }

        onPinchUpdated: {
            let factor = pinch.scale / lastPinchScale;
            if (factor !== 1.0) {
                videoWindow.zoomAt(factor, pinch.center);
            }
            lastPinchScale = pinch.scale;
        }
    }

    PointHandler {
        id: pointHandler
        acceptedButtons: Qt.LeftButton
        // Enable this handler only when zoomed and NOT doing a selection drag
        enabled: videoWindow.isZoomed && !mainWindow.isCtrlPressed

        property point lastPosition: Qt.point(0, 0)

        onActiveChanged: {
            isMouseDown = active;
            if (active) {
                lastPosition = point.position;
            }
        }

        onPointChanged: {
            if (active) {
                var currentPosition = point.position;
                var delta = Qt.point(currentPosition.x - lastPosition.x, currentPosition.y - lastPosition.y);

                if (delta.x !== 0 || delta.y !== 0) {
                    videoWindow.pan(delta);
                }

                lastPosition = currentPosition;
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
                videoWindow.zoomAt(factor, wheelHandler.point.position);
            }

            wheelHandler.lastRotation = wheelHandler.rotation;
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        hoverEnabled: true // Needed for cursor shape changes

        cursorShape: {
            if (mainWindow.isCtrlPressed)
                return Qt.CrossCursor;
            if (videoWindow.isZoomed) {
               return (videoWindow.isMouseDown) ? Qt.ClosedHandCursor : Qt.OpenHandCursor;
            }
            return Qt.ArrowCursor;
        }

        onPressed: function (mouse) {
            if (mainWindow.isCtrlPressed) {
                // Force reset processing state, ensure new selection can start
                isProcessingSelection = false;
                isSelecting = true;
                selectionStart = Qt.point(mouse.x, mouse.y);
                selectionEnd = selectionStart;
            } else {
                mouse.accepted = false;
            }
        }

        onPositionChanged: function (mouse) {
            if (videoWindow.isSelecting) {
                var currentPos = Qt.point(mouse.x, mouse.y);
                var deltaX = currentPos.x - selectionStart.x;
                var deltaY = currentPos.y - selectionStart.y;

                if (deltaX === 0 && deltaY === 0)
                    return;
                selectionEnd = Qt.point(selectionStart.x + deltaX, selectionStart.y + deltaY);
                selectionCanvas.requestPaint();
            }
        }

        onReleased: function (mouse) {
            if (videoWindow.isSelecting) {
                isSelecting = false;
                isProcessingSelection = true;

                // Calculate rectangle area
                var rect = Qt.rect(Math.min(selectionStart.x, selectionEnd.x), Math.min(selectionStart.y, selectionEnd.y), Math.abs(selectionEnd.x - selectionStart.x), Math.abs(selectionEnd.y - selectionStart.y));

                // console.log("Final selection rect:", rect.x, rect.y, rect.width, rect.height);

                videoWindow.zoomToSelection(rect);

                // Clear selection state, make rectangle disappear
                selectionStart = Qt.point(0, 0);
                selectionEnd = Qt.point(0, 0);
                selectionCanvas.requestPaint();
                isProcessingSelection = false;
            }
        }
    }

    // Draw selection rectangle
    Canvas {
        id: selectionCanvas
        anchors.fill: parent
        z: 1

        onPaint: {
            var ctx = getContext("2d");
            ctx.clearRect(0, 0, width, height);
            if (videoWindow.isSelecting || (videoWindow.selectionStart.x !== 0 || videoWindow.selectionStart.y !== 0)) {
                var rect = Qt.rect(Math.min(videoWindow.selectionStart.x, videoWindow.selectionEnd.x), Math.min(videoWindow.selectionStart.y, videoWindow.selectionEnd.y), Math.abs(videoWindow.selectionEnd.x - videoWindow.selectionStart.x), Math.abs(videoWindow.selectionEnd.y - videoWindow.selectionStart.y));
                ctx.fillStyle = "rgba(0, 0, 255, 0.2)";
                ctx.fillRect(rect.x, rect.y, rect.width, rect.height);
                ctx.strokeStyle = "blue";
                ctx.lineWidth = 2;
                ctx.strokeRect(rect.x, rect.y, rect.width, rect.height);
            }
        }
    }

    function resetSelectionCanvas() {
        selectionCanvas.requestPaint();
    }
}