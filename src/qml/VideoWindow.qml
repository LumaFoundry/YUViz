import QtQuick 6.0
import QtQuick.Controls.Basic 6.0
import Window 1.0

VideoWindow {
    id: videoWindow
    property var videoId: ""
    objectName: "videoWindow_" + videoId
    signal requestRemove

    Button {
        text: "✕"
        width: 30
        height: 30
        anchors.top: parent.top
        anchors.right: parent.right
        onClicked: {
            videoWindow.requestRemove();
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
        enabled: videoWindow.isZoomed && !isCtrlPressed

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
            if (isCtrlPressed)
                return Qt.CrossCursor;
            if (videoWindow.isZoomed) {
                return (isMouseDown) ? Qt.ClosedHandCursor : Qt.OpenHandCursor;
            }
            return Qt.ArrowCursor;
        }

        onPressed: function (mouse) {
            if (isCtrlPressed) {
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
            if (isSelecting) {
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
            if (isSelecting) {
                isSelecting = false;
                isProcessingSelection = true;

                // Calculate rectangle area
                var rect = Qt.rect(Math.min(selectionStart.x, selectionEnd.x), Math.min(selectionStart.y, selectionEnd.y), Math.abs(selectionEnd.x - selectionStart.x), Math.abs(selectionEnd.y - selectionStart.y));

                // console.log("Final selection rect:", rect.x, rect.y, rect.width, rect.height);

                videoWindow.setSelectionRect(rect);
                videoWindow.zoomToSelection();

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
            if (isSelecting || (selectionStart.x !== 0 || selectionStart.y !== 0)) {
                var rect = Qt.rect(Math.min(selectionStart.x, selectionEnd.x), Math.min(selectionStart.y, selectionEnd.y), Math.abs(selectionEnd.x - selectionStart.x), Math.abs(selectionEnd.y - selectionStart.y));
                ctx.fillStyle = "rgba(0, 0, 255, 0.2)";
                ctx.fillRect(rect.x, rect.y, rect.width, rect.height);
                ctx.strokeStyle = "blue";
                ctx.lineWidth = 2;
                ctx.strokeRect(rect.x, rect.y, rect.width, rect.height);
            }
        }
    }

    // Pixel values overlay
    Canvas {
        id: pixelValuesCanvas
        anchors.fill: parent
        z: 2
        visible: true

        property var frameMeta: null
        property var pixelValueThreshold: 10

        // connect to VideoWindow signals
        Connections {
            target: videoWindow
            function onFrameReady() {
                pixelValuesCanvas.requestPaint()
            }
            function onZoomChanged() {
                pixelValuesCanvas.requestPaint()
            }
            function onCenterXChanged() {
                pixelValuesCanvas.requestPaint()
            }
            function onCenterYChanged() {
                pixelValuesCanvas.requestPaint()
            }
        }

        onPaint: {
            try {
                var ctx = getContext("2d");
                if (!ctx) return;
                
                ctx.clearRect(0, 0, width, height);
                
                // get frame meta
                var meta = videoWindow.getFrameMeta();
                if (!meta || !meta.yWidth || !meta.yHeight) return;
                var yWidth = meta.yWidth;
                var yHeight = meta.yHeight;
                var uvWidth = meta.uvWidth;
                var uvHeight = meta.uvHeight;
                var format = meta.format;

                // threshold logic
                var threshold = Math.round(yWidth / 10);
                if (videoWindow.zoom < threshold) return;

                var videoRect = getVideoRect();
                var pixelSpacing = Math.max(1, Math.floor(40 / videoWindow.zoom));
                var startX = Math.max(0, Math.floor((videoWindow.centerX - 0.6 / videoWindow.zoom) * yWidth));
                var endX = Math.min(yWidth, Math.ceil((videoWindow.centerX + 0.6 / videoWindow.zoom) * yWidth));
                var startY = Math.max(0, Math.floor((videoWindow.centerY - 0.6 / videoWindow.zoom) * yHeight));
                var endY = Math.min(yHeight, Math.ceil((videoWindow.centerY + 0.6 / videoWindow.zoom) * yHeight));

                for (var y = startY; y < endY; y += pixelSpacing) {
                    for (var x = startX; x < endX; x += pixelSpacing) {
                        if (x >= yWidth || y >= yHeight) continue;
                        var yuv = videoWindow.getYUV(x, y);
                        if (!yuv || yuv.length !== 3) continue;
                        var yValue = yuv[0];
                        var uValue = yuv[1];
                        var vValue = yuv[2];
                        var normalizedX = (x + 0.5) / yWidth;
                        var normalizedY = (y + 0.5) / yHeight;
                        var transformedX = (normalizedX - videoWindow.centerX) * videoWindow.zoom + 0.5;
                        var transformedY = (normalizedY - videoWindow.centerY) * videoWindow.zoom + 0.5;
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
            var itemRect = {width: width, height: height};
            var videoRect = {x: 0, y: 0, width: itemRect.width, height: itemRect.height};
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
                var fontSize = Math.max(10, Math.min(15, 0.8 * videoWindow.zoom));
                var padding = Math.max(3, Math.min(9, 0.3 * videoWindow.zoom));
                var lineHeight = fontSize + 2;
                
                var text = "Y" + yVal + "\nU" + uVal + "\nV" + vVal;
                var lines = text.split('\n');
                var maxWidth = 0;
                
                // temporarily set font to calculate text width
                var originalFont = ctx.font;
                ctx.font = fontSize + "px Consolas";
                
                for (var i = 0; i < lines.length; i++) {
                    var textWidth = ctx.measureText(lines[i]).width;
                    maxWidth = Math.max(maxWidth, textWidth);
                }
                
                // draw semi-transparent background
                var bgWidth = maxWidth + padding * 2;
                var bgHeight = lines.length * lineHeight + padding * 2;
                ctx.fillStyle = "rgba(0, 0, 0, 0.3)";
                ctx.fillRect(x - bgWidth/2, y - bgHeight/2, bgWidth, bgHeight);
                
                // draw text
                ctx.textAlign = "center";
                ctx.textBaseline = "middle";
                
                for (var i = 0; i < lines.length; i++) {
                    var textY = y - bgHeight/2 + padding + i * lineHeight + lineHeight/2;
                    
                    // draw black stroke
                    ctx.strokeStyle = "black";
                    ctx.lineWidth = 1;
                    ctx.strokeText(lines[i], x, textY);
                    
                    // draw white text
                    ctx.fillStyle = "white";
                    ctx.fillText(lines[i], x, textY);
                }
                
                // restore original font
                ctx.font = originalFont;
            } catch (error) {
                console.log("QML: drawPixelValue error:", error);
            }
        }
    }

    function resetSelectionCanvas() {
        selectionCanvas.requestPaint();
    }
}
