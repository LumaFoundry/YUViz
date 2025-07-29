import QtQuick 6.0
import QtQuick.Controls.Basic 6.0
import DiffWindow 1.0
import QtQuick.Layouts 1.15

Window {
    id: diffWindow
    width: 500
    height: 400
    visible: false
    title: "Diff Result"
    flags: Qt.Window
    color: "black"

    property point dragPos
    property int leftVideoId: -1
    property int rightVideoId: -1
    property int diffVideoId: -1
    property string psnrInfo: compareController.psnrInfo
    property alias diffVideoWindow: diffVideoWindow

    // Zoom and selection properties
    property bool isSelecting: false
    property point selectionStart: Qt.point(0, 0)
    property point selectionEnd: Qt.point(0, 0)
    property bool isProcessingSelection: false
    property bool isMouseDown: false
    property bool isZoomed: false  
    property bool isCtrlPressed: mainWindow ? mainWindow.isCtrlPressed : false

    DiffWindow {
        id: diffVideoWindow
        objectName: "diffWindow"
        anchors.fill: parent

        // Update isZoomed property when sharedView changes
        Connections {
            target: diffVideoWindow.sharedView
            enabled: diffVideoWindow.sharedView !== null
            function onIsZoomedChanged() {
                if (diffVideoWindow.sharedView) {
                    diffWindow.isZoomed = diffVideoWindow.sharedView.isZoomed;
                }
            }
        }

        // Update isCtrlPressed property when mainWindow changes
        Connections {
            target: mainWindow
            enabled: mainWindow !== null
            function onIsCtrlPressedChanged() {
                if (mainWindow) {
                    diffWindow.isCtrlPressed = mainWindow.isCtrlPressed;
                }
            }
        }

        // Configuration menu
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
                        contentItem: ColumnLayout {
                            spacing: 8
                            width: 200

                            Text {
                                text: "Display Mode"
                                color: "black"
                                font.pixelSize: 12
                            }

                            ComboBox {
                                id: displayModeSelector
                                model: ["Grayscale", "Heatmap", "Binary"]
                                width: 180
                                currentIndex: diffVideoWindow.displayMode

                                onCurrentIndexChanged: {
                                    diffVideoWindow.displayMode = currentIndex;
                                    keyHandler.forceActiveFocus();
                                }

                                onActivated: keyHandler.forceActiveFocus()
                            }

                            Text {
                                text: "Diff Method"
                                color: "black"
                                font.pixelSize: 12
                            }

                            ComboBox {
                                id: diffMethodSelector
                                model: ["Direct Subtraction", "Squared Difference", "Normalized", "Absolute Difference"]
                                width: 180
                                currentIndex: diffVideoWindow.diffMethod

                                onCurrentIndexChanged: {
                                    diffVideoWindow.diffMethod = currentIndex;
                                    keyHandler.forceActiveFocus();
                                }

                                onActivated: keyHandler.forceActiveFocus()
                            }

                            Text {
                                text: "Multiplier"
                                color: "black"
                                font.pixelSize: 12
                            }

                            TextField {
                                id: multiplierInput
                                width: 180
                                text: diffVideoWindow.diffMultiplier.toString()
                                color: "black"
                                background: Rectangle {
                                    color: "transparent"
                                    border.color: "black"
                                    border.width: 1
                                }

                                onEditingFinished: {
                                    let value = parseFloat(text);
                                    if (!isNaN(value) && value > 0) {
                                        diffVideoWindow.diffMultiplier = value;
                                    } else {
                                        text = diffVideoWindow.diffMultiplier.toString();
                                    }
                                    keyHandler.forceActiveFocus();
                                }

                                onAccepted: keyHandler.forceActiveFocus()
                            }
                        }
                    }

                    MenuSeparator {
                    }

                    MenuItem {
                        text: "Close Diff Window"
                        onTriggered: diffWindow.close()
                    }
                }
            }
        }

        // Zoom and pan handlers
        PinchArea {
            anchors.fill: parent
            pinch.target: diffVideoWindow

            property real lastPinchScale: 1.0

            onPinchStarted: {
                lastPinchScale = 1.0;
            }

            onPinchUpdated: {
                let factor = pinch.scale / lastPinchScale;
                if (factor !== 1.0) {
                    diffVideoWindow.zoomAt(factor, pinch.center);
                }
                lastPinchScale = pinch.scale;
            }
        }

        PointHandler {
            id: pointHandler
            acceptedButtons: Qt.LeftButton
            // Enable this handler only when zoomed and NOT doing a selection drag
            enabled: (diffWindow && diffWindow.isZoomed && !diffWindow.isCtrlPressed) || false

            property point lastPosition: Qt.point(0, 0)

            onActiveChanged: {
                if (diffWindow) {
                    diffWindow.isMouseDown = active;
                }
                if (active) {
                    lastPosition = point.position;
                }
            }

            onPointChanged: {
                if (active) {
                    var currentPosition = point.position;
                    var delta = Qt.point(currentPosition.x - lastPosition.x, currentPosition.y - lastPosition.y);

                    if (delta.x !== 0 || delta.y !== 0) {
                        diffVideoWindow.pan(delta);
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
                    diffVideoWindow.zoomAt(factor, wheelHandler.point.position);
                }

                wheelHandler.lastRotation = wheelHandler.rotation;
            }
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            hoverEnabled: true // Needed for cursor shape changes

            cursorShape: {
                if (diffWindow && diffWindow.isCtrlPressed)
                    return Qt.CrossCursor;
                if (diffWindow && diffWindow.isZoomed) {
                    return (diffWindow.isMouseDown) ? Qt.ClosedHandCursor : Qt.OpenHandCursor;
                }
                return Qt.ArrowCursor;
            }

            onPressed: function (mouse) {
                if (diffWindow && diffWindow.isCtrlPressed) {
                    // Force reset processing state, ensure new selection can start
                    diffWindow.isProcessingSelection = false;
                    diffWindow.isSelecting = true;
                    diffWindow.selectionStart = Qt.point(mouse.x, mouse.y);
                    diffWindow.selectionEnd = diffWindow.selectionStart;
                } else {
                    mouse.accepted = false;
                }
            }

            onPositionChanged: function (mouse) {
                if (diffWindow && diffWindow.isSelecting) {
                    var currentPos = Qt.point(mouse.x, mouse.y);
                    var deltaX = currentPos.x - diffWindow.selectionStart.x;
                    var deltaY = currentPos.y - diffWindow.selectionStart.y;

                    if (deltaX === 0 && deltaY === 0)
                        return;
                    diffWindow.selectionEnd = Qt.point(diffWindow.selectionStart.x + deltaX, diffWindow.selectionStart.y + deltaY);
                    selectionCanvas.requestPaint();
                }
            }

            onReleased: function (mouse) {
                if (diffWindow && diffWindow.isSelecting) {
                    diffWindow.isSelecting = false;
                    diffWindow.isProcessingSelection = true;

                    // Calculate rectangle area
                    var rect = Qt.rect(Math.min(diffWindow.selectionStart.x, diffWindow.selectionEnd.x), Math.min(diffWindow.selectionStart.y, diffWindow.selectionEnd.y), Math.abs(diffWindow.selectionEnd.x - diffWindow.selectionStart.x), Math.abs(diffWindow.selectionEnd.y - diffWindow.selectionStart.y));

                    // console.log("Final selection rect:", rect.x, rect.y, rect.width, rect.height);

                    diffVideoWindow.zoomToSelection(rect);

                    // Clear selection state, make rectangle disappear
                    diffWindow.selectionStart = Qt.point(0, 0);
                    diffWindow.selectionEnd = Qt.point(0, 0);
                    selectionCanvas.requestPaint();
                    diffWindow.isProcessingSelection = false;
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
                if (diffWindow && (diffWindow.isSelecting || (diffWindow.selectionStart.x !== 0 || diffWindow.selectionStart.y !== 0))) {
                    var rect = Qt.rect(Math.min(diffWindow.selectionStart.x, diffWindow.selectionEnd.x), Math.min(diffWindow.selectionStart.y, diffWindow.selectionEnd.y), Math.abs(diffWindow.selectionEnd.x - diffWindow.selectionStart.x), Math.abs(diffWindow.selectionEnd.y - diffWindow.selectionStart.y));
                    ctx.fillStyle = "rgba(0, 0, 255, 0.2)";
                    ctx.fillRect(rect.x, rect.y, rect.width, rect.height);
                    ctx.strokeStyle = "blue";
                    ctx.lineWidth = 2;
                    ctx.strokeRect(rect.x, rect.y, rect.width, rect.height);
                }
            }
        }

        // Pixel difference values canvas
        Canvas {
            id: pixelValuesCanvas
            anchors.fill: parent
            z: 2
            visible: true

            property var frameMeta: null
            property var pixelValueThreshold: 10

            // connect to DiffWindow signals
            Connections {
                target: diffVideoWindow.sharedView

                function onViewChanged() {
                    pixelValuesCanvas.requestPaint();
                }
            }
            Connections {
                target: diffVideoWindow

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

                    // Check if sharedView exists
                    if (!diffVideoWindow.sharedView) {
                        return;
                    }

                    // get frame meta
                    var meta = diffVideoWindow.getFrameMeta();
                    if (!meta || !meta.yWidth || !meta.yHeight)
                        return;
                    var yWidth = meta.yWidth;
                    var yHeight = meta.yHeight;

                    // threshold logic
                    var threshold = Math.round(yWidth / 10);
                    if (diffVideoWindow.sharedView.zoom < threshold)
                        return;

                    var videoRect = getVideoRect();
                    var pixelSpacing = Math.max(1, Math.floor(40 / diffVideoWindow.sharedView.zoom));
                    var startX = Math.max(0, Math.floor((diffVideoWindow.sharedView.centerX - 0.6 / diffVideoWindow.sharedView.zoom) * yWidth));
                    var endX = Math.min(yWidth, Math.ceil((diffVideoWindow.sharedView.centerX + 0.6 / diffVideoWindow.sharedView.zoom) * yWidth));
                    var startY = Math.max(0, Math.floor((diffVideoWindow.sharedView.centerY - 0.6 / diffVideoWindow.sharedView.zoom) * yHeight));
                    var endY = Math.min(yHeight, Math.ceil((diffVideoWindow.sharedView.centerY + 0.6 / diffVideoWindow.sharedView.zoom) * yHeight));

                    // Additional bounds checking
                    if (startX < 0 || startY < 0 || endX > yWidth || endY > yHeight) {
                        console.log("QML: Invalid bounds detected");
                        return;
                    }

                    for (var y = startY; y < endY; y += pixelSpacing) {
                        for (var x = startX; x < endX; x += pixelSpacing) {
                            if (x >= yWidth || y >= yHeight)
                                continue;
                            try {
                                var diffValues = diffVideoWindow.getDiffValue(x, y);
                                if (!diffValues || diffValues.length !== 3)
                                    continue;
                                var y1Value = diffValues[0];
                                var y2Value = diffValues[1];
                                var diffValue = diffValues[2];
                                
                                // Additional safety checks
                                if (typeof y1Value !== 'number' || typeof y2Value !== 'number' || typeof diffValue !== 'number')
                                    continue;
                                var normalizedX = (x + 0.5) / yWidth;
                                var normalizedY = (y + 0.5) / yHeight;
                                var transformedX = (normalizedX - diffVideoWindow.sharedView.centerX) * diffVideoWindow.sharedView.zoom + 0.5;
                                var transformedY = (normalizedY - diffVideoWindow.sharedView.centerY) * diffVideoWindow.sharedView.zoom + 0.5;
                                var screenX = videoRect.x + transformedX * videoRect.width;
                                var screenY = videoRect.y + transformedY * videoRect.height;
                                if (screenX >= 0 && screenX < width && screenY >= 0 && screenY < height) {
                                    drawPixelValue(ctx, screenX, screenY, y1Value, y2Value, diffValue);
                                }
                            } catch (error) {
                                console.log("QML: Error processing pixel at", x, y, ":", error);
                                continue;
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
                var videoAspect = diffVideoWindow.getAspectRatio;

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

            function drawPixelValue(ctx, x, y, y1Val, y2Val, diffVal) {
                try {
                    var fontSize = Math.max(10, Math.min(15, 0.8 * diffVideoWindow.sharedView.zoom));
                    var padding = Math.max(3, Math.min(9, 0.3 * diffVideoWindow.sharedView.zoom));
                    var lineHeight = fontSize + 2;

                    var text = "Y1:" + y1Val + "\nY2:" + y2Val + "\nDiff:" + diffVal;
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

        // OSD Overlay
        Rectangle {
            visible: diffVideoWindow.osdState > 0
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.margins: 10
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
                    var osdStr = diffWindow.psnrInfo;

                    if (diffVideoWindow.osdState === 1) {
                        // Basic info: PSNR
                        return osdStr;
                    } else if (diffVideoWindow.osdState === 2) {
                        // Detailed info: Could add later
                        return osdStr;
                    }
                    return "";
                }
            }
        }
    }

    MouseArea {
        onPressed: function (mouse) {
            dragPos = Qt.point(mouse.x, mouse.y);
        }
        onPositionChanged: function (mouse) {
            if (mouse.buttons & Qt.LeftButton) {
                diffWindow.x += mouse.x - dragPos.x;
                diffWindow.y += mouse.y - dragPos.y;
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
}
