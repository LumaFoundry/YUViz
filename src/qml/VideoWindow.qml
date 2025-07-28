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
    property bool isCtrlPressed: mainWindow.isCtrlPressed
    property bool isZoomed: sharedView ? sharedView.isZoomed : false
    property var sharedView: sharedViewProperties
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
            indicator: Canvas {
            }
        }

        Button {
            id: menuButton
            width: 40
            height: 40
            text: "⚙"
            font.pixelSize: 30
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
                                {
                                    space: 1,
                                    range: 1
                                }  // BT709 MPEG
                                ,
                                {
                                    space: 1,
                                    range: 2
                                }  // BT709 Full
                                ,
                                {
                                    space: 5,
                                    range: 1
                                }  // BT470BG MPEG
                                ,
                                {
                                    space: 5,
                                    range: 2
                                }  // BT470BG Full
                                ,
                                {
                                    space: 10,
                                    range: 1
                                } // BT2020_CL MPEG
                                ,
                                {
                                    space: 10,
                                    range: 2
                                }  // BT2020_CL Full
                            ];
                            let selected = colorSpaceMap[currentIndex];
                            videoWindow.setColorParams(selected.space, selected.range);
                            keyHandler.forceActiveFocus();
                        }

                        onActivated: keyHandler.forceActiveFocus()
                    }
                }

                MenuSeparator {
                }

                MenuItem {
                    text: "Close the Video"
                    onTriggered: videoWindow.requestRemove(videoWindow.videoId)
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

    Canvas {
        id: pixelValuesCanvas
        anchors.fill: parent
        z: 2
        visible: true

        property var frameMeta: null
        property var pixelValueThreshold: 10

        // connect to VideoWindow signals
        Connections {
            target: sharedViewProperties

            function onViewChanged() {
                pixelValuesCanvas.requestPaint();
            }
        }
        Connections {
            target: videoWindow

            function onFrameReady() {
                pixelValuesCanvas.requestPaint();
            }
        }

        onPaint: {
            try {
                var ctx = getContext("2d");
                if (!ctx)
                    return;

                ctx.clearRect(0, 0, width, height);

                // get frame meta
                var meta = videoWindow.getFrameMeta();
                if (!meta || !meta.yWidth || !meta.yHeight)
                    return;
                var yWidth = meta.yWidth;
                var yHeight = meta.yHeight;
                var uvWidth = meta.uvWidth;
                var uvHeight = meta.uvHeight;
                var format = meta.format;

                // threshold logic
                var threshold = Math.round(yWidth / 10);
                if (videoWindow.sharedView.zoom < threshold)
                    return;

                var videoRect = getVideoRect();
                var pixelSpacing = Math.max(1, Math.floor(40 / videoWindow.sharedView.zoom));
                var startX = Math.max(0, Math.floor((videoWindow.sharedView.centerX - 0.6 / videoWindow.sharedView.zoom) * yWidth));
                var endX = Math.min(yWidth, Math.ceil((videoWindow.sharedView.centerX + 0.6 / videoWindow.sharedView.zoom) * yWidth));
                var startY = Math.max(0, Math.floor((videoWindow.sharedView.centerY - 0.6 / videoWindow.sharedView.zoom) * yHeight));
                var endY = Math.min(yHeight, Math.ceil((videoWindow.sharedView.centerY + 0.6 / videoWindow.sharedView.zoom) * yHeight));

                for (var y = startY; y < endY; y += pixelSpacing) {
                    for (var x = startX; x < endX; x += pixelSpacing) {
                        if (x >= yWidth || y >= yHeight)
                            continue;
                        var yuv = videoWindow.getYUV(x, y);
                        if (!yuv || yuv.length !== 3)
                            continue;
                        var yValue = yuv[0];
                        var uValue = yuv[1];
                        var vValue = yuv[2];
                        var normalizedX = (x + 0.5) / yWidth;
                        var normalizedY = (y + 0.5) / yHeight;
                        var transformedX = (normalizedX - videoWindow.sharedView.centerX) * videoWindow.sharedView.zoom + 0.5;
                        var transformedY = (normalizedY - videoWindow.sharedView.centerY) * videoWindow.sharedView.zoom + 0.5;
                        var screenX = videoRect.x + transformedX * videoRect.width;
                        var screenY = videoRect.y + transformedY * videoRect.height;
                        if (screenX >= 0 && screenX < width && screenY >= 0 && screenY < height) {
                            drawPixelValue(ctx, screenX, screenY, yValue, uValue, vValue);
                        }
                    }
                }
            } catch (error) {
                console.log("QML: Canvas onPaint error:", error);
            }
        }

        function getVideoRect() {
            var itemRect = {
                width: width,
                height: height
            };
            var videoRect = {
                x: 0,
                y: 0,
                width: itemRect.width,
                height: itemRect.height
            };
            var windowAspect = itemRect.width / itemRect.height;
            var videoAspect = videoWindow.getAspectRatio;

            if (windowAspect > videoAspect) {
                // window wider, height adapted, width centered
                var newWidth = videoAspect * itemRect.height;
                videoRect.x = (itemRect.width - newWidth) / 2.0;
                videoRect.width = newWidth;
            } else {
                // window higher, width adapted, height centered
                var newHeight = itemRect.width / videoAspect;
                videoRect.y = (itemRect.height - newHeight) / 2.0;
                videoRect.height = newHeight;
            }

            return videoRect;
        }

        function drawPixelValue(ctx, x, y, yVal, uVal, vVal) {
            try {
                var fontSize = Math.max(10, Math.min(15, 0.8 * videoWindow.sharedView.zoom));
                var padding = Math.max(3, Math.min(9, 0.3 * videoWindow.sharedView.zoom));
                var lineHeight = fontSize + 2;

                var text = "Y" + yVal + "\nU" + uVal + "\nV" + vVal;
                var lines = text.split('\n');
                var maxWidth = 0;

                // temporarily set font to calculate text width
                ctx.font = Math.floor(fontSize) + "px Consolas";

                for (var i = 0; i < lines.length; i++) {
                    var textWidth = ctx.measureText(lines[i]).width;
                    maxWidth = Math.max(maxWidth, textWidth);
                }

                // draw semi-transparent background
                var bgWidth = maxWidth + padding * 2;
                var bgHeight = lines.length * lineHeight + padding * 2;
                ctx.fillStyle = "rgba(0, 0, 0, 0.3)";
                ctx.fillRect(x - bgWidth / 2, y - bgHeight / 2, bgWidth, bgHeight);

                // draw text
                ctx.textAlign = "center";
                ctx.textBaseline = "middle";

                for (var i = 0; i < lines.length; i++) {
                    var textY = y - bgHeight / 2 + padding + i * lineHeight + lineHeight / 2;

                    // draw black stroke
                    ctx.strokeStyle = "black";
                    ctx.lineWidth = 1;
                    ctx.strokeText(lines[i], x, textY);

                    // draw white text
                    ctx.fillStyle = "white";
                    ctx.fillText(lines[i], x, textY);
                }

                // restore original font
                ctx.font = "12px monospace";
            } catch (error) {
                console.log("QML: drawPixelValue error:", error);
            }
        }
    }

    function resetSelectionCanvas() {
        selectionStart = Qt.point(0, 0);
        selectionEnd = Qt.point(0, 0);
        isSelecting = false;
        isProcessingSelection = false;
        selectionCanvas.requestPaint();
    }

    // OSD Overlay
    Rectangle {
        visible: videoWindow.osdState > 0
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.bottomMargin: 30
        anchors.rightMargin: 10
        width: osdText.width + 20
        height: osdText.height + 20
        color: "black"
        opacity: 0.8
        radius: 5
        z: 100 // Ensure it's on top

        Text {
            id: osdText
            anchors.centerIn: parent
            color: "white"
            font.family: "monospace"
            font.pixelSize: 12
            text: {
                if (videoWindow.osdState === 1) {
                    // Basic info: duration and current frame / total frames
                    var durationSec = Math.floor(videoWindow.duration / 1000);
                    var currentSec = Math.floor(videoWindow.currentTimeMs / 1000);
                    var durationMin = Math.floor(durationSec / 60);
                    var currentMin = Math.floor(currentSec / 60);
                    var durationStr = durationMin + ":" + (durationSec % 60).toString().padStart(2, '0');
                    var currentStr = currentMin + ":" + (currentSec % 60).toString().padStart(2, '0');

                    return "Time: " + currentStr + " / " + durationStr + "\n" + "Frame: " + videoWindow.currentFrame
                        + " / " + videoWindow.totalFrames;
                } else if (videoWindow.osdState === 2) {
                    // Detailed info: add pixel format, timebase/pts, aspect ratio
                    var durationSec = Math.floor(videoWindow.duration / 1000);
                    var currentSec = Math.floor(videoWindow.currentTimeMs / 1000);
                    var durationMin = Math.floor(durationSec / 60);
                    var currentMin = Math.floor(currentSec / 60);
                    var durationStr = durationMin + ":" + (durationSec % 60).toString().padStart(2, '0');
                    var currentStr = currentMin + ":" + (currentSec % 60).toString().padStart(2, '0');

                    return "Time: " + currentStr + " / " + durationStr + "\n" + "Frame: " + videoWindow.currentFrame
                        + " / " + videoWindow.totalFrames + "\n" + "Timebase: " + videoWindow.timeBase + "\n" + "Aspect: "
                        + videoWindow.getAspectRatio.toFixed(2) + "\n" + "Color Space: "
                        + videoWindow.colorSpace + "\n" + "Color Range: " + videoWindow.colorRange;
                }
                return "";
            }
        }
    }
}
