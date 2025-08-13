import QtQuick 6.0
import QtQuick.Controls.Basic 6.0
import DiffWindow 1.0
import QtQuick.Layouts 1.15

Item {
    id: diffPane
    objectName: "diffEmbeddedInstance"
    property alias diffVideoWindow: diffVideoWindow
    property bool wasPlayingBeforeResize: false

    // Legacy transient selection (unused after persistent rectangle logic migrated)
    property bool isSelecting: false
    property point selectionStart: Qt.point(0, 0)
    property point selectionEnd: Qt.point(0, 0)
    property bool isProcessingSelection: false
    property bool isMouseDown: false
    property bool isZoomed: diffVideoWindow.sharedView ? diffVideoWindow.sharedView.isZoomed : false
    property bool isCtrlPressed: mainWindow ? mainWindow.isCtrlPressed : false
    property bool resizing: false
    property string psnrInfo: compareController.psnrInfo

    // Persistent rectangle state (stored in video pixel coordinates)
    property bool hasPersistentRect: false
    property rect persistentRect: Qt.rect(0, 0, 0, 0)
    property bool isPersistentRectSelecting: false
    property point persistentRectStart: Qt.point(0, 0)
    property point persistentRectEnd: Qt.point(0, 0)

    // Rectangle dragging state
    property bool isDraggingPersistentRect: false
    property point dragStartOffset: Qt.point(0, 0)

    // Rectangle resizing state
    property bool isResizingRect: false
    property string resizeHandle: ""
    property point resizeStartPoint: Qt.point(0, 0)
    property rect resizeStartRect: Qt.rect(0, 0, 0, 0)

    DiffWindow {
        id: diffVideoWindow
        objectName: "diffWindow"
        anchors.fill: parent

        Connections {
            target: diffVideoWindow.sharedView
            enabled: diffVideoWindow.sharedView !== null
            function onIsZoomedChanged() {
                diffPane.isZoomed = diffVideoWindow.sharedView.isZoomed;
            }
        }
        Connections {
            target: mainWindow
            enabled: mainWindow !== null
            function onIsCtrlPressedChanged() {
                diffPane.isCtrlPressed = mainWindow.isCtrlPressed;
                // Ensure only one rectangle exists globally: clear embedded rectangle when Ctrl is pressed
                if (mainWindow.isCtrlPressed && diffPane.hasPersistentRect) {
                    removePersistentRect();
                }
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
                onClicked: {
                    menu.open();
                    keyHandler.forceActiveFocus();
                }

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
                                    let v = parseFloat(text);
                                    text = (!isNaN(v) && v > 0) ? (diffVideoWindow.diffMultiplier = v, text) : diffVideoWindow.diffMultiplier.toString();
                                    keyHandler.forceActiveFocus();
                                }
                                onAccepted: keyHandler.forceActiveFocus()
                            }
                        }
                    }

                    MenuSeparator {}

                    MenuItem {
                        visible: mainWindow && mainWindow.isEmbeddedDiffActive()
                        text: "Restore second video"
                        onTriggered: {
                            mainWindow.disableEmbeddedDiffAndRestore();
                            keyHandler.forceActiveFocus();
                        }
                    }
                }
            }
        }

        PinchArea {
            anchors.fill: parent
            pinch.target: diffVideoWindow
            property real lastPinchScale: 1.0
            onPinchStarted: lastPinchScale = 1.0
            onPinchUpdated: {
                let f = pinch.scale / lastPinchScale;
                if (f !== 1.0)
                    diffVideoWindow.zoomAt(f, pinch.center);
                lastPinchScale = pinch.scale;
            }
        }
        PointHandler {
            id: pointHandler
            acceptedButtons: Qt.LeftButton
            enabled: (diffPane.isZoomed && !diffPane.isCtrlPressed) || false
            property point lastPosition: Qt.point(0, 0)
            onActiveChanged: {
                diffPane.isMouseDown = active;
                if (active)
                    lastPosition = point.position;
            }
            onPointChanged: {
                if (active) {
                    var cur = point.position;
                    var delta = Qt.point(cur.x - lastPosition.x, cur.y - lastPosition.y);
                    if (delta.x || delta.y)
                        diffVideoWindow.pan(delta);
                    lastPosition = cur;
                }
            }
        }
        WheelHandler {
            id: wheelHandler
            acceptedModifiers: Qt.ControlModifier
            property real lastRotation: wheelHandler.rotation
            onRotationChanged: {
                let d = wheelHandler.rotation - wheelHandler.lastRotation;
                if (d)
                    diffVideoWindow.zoomAt(d > 0 ? 1.2 : 1.0 / 1.2, wheelHandler.point.position);
                wheelHandler.lastRotation = wheelHandler.rotation;
            }
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            hoverEnabled: true
            cursorShape: {
                // Show resize cursors when hovering over handles
                if (diffPane.hasPersistentRect) {
                    var screenRect = convertVideoToScreenCoordinates(diffPane.persistentRect);
                    var handle = getResizeHandleAtPoint(Qt.point(mouseX, mouseY), screenRect);
                    switch (handle) {
                    case "nw":
                    case "se":
                        return Qt.SizeFDiagCursor;
                    case "ne":
                    case "sw":
                        return Qt.SizeBDiagCursor;
                    case "n":
                    case "s":
                        return Qt.SizeVerCursor;
                    case "w":
                    case "e":
                        return Qt.SizeHorCursor;
                    }
                }
                if (diffPane.isCtrlPressed)
                    return Qt.CrossCursor;
                if (diffPane.isZoomed) {
                    return (diffPane.isMouseDown) ? Qt.ClosedHandCursor : Qt.OpenHandCursor;
                }
                return Qt.ArrowCursor;
            }
            onPressed: function (mouse) {
                // Determine Ctrl state from event modifiers to avoid stale state
                var ctrlDown = (mouse.modifiers & Qt.ControlModifier) !== 0;
                diffPane.isCtrlPressed = ctrlDown;
                if (ctrlDown) {
                    // Clear existing rectangle before starting a new selection
                    if (diffPane.hasPersistentRect) {
                        removePersistentRect();
                    }
                    diffPane.isPersistentRectSelecting = true;
                    var videoPoint = convertScreenToPixelCoordinates(Qt.point(mouse.x, mouse.y));
                    diffPane.persistentRectStart = videoPoint;
                    diffPane.persistentRectEnd = videoPoint;
                } else if (diffPane.hasPersistentRect && !diffPane.isPersistentRectSelecting) {
                    var screenPoint = Qt.point(mouse.x, mouse.y);
                    var screenRect = convertVideoToScreenCoordinates(diffPane.persistentRect);
                    var handle = getResizeHandleAtPoint(screenPoint, screenRect);
                    if (handle !== "") {
                        diffPane.isResizingRect = true;
                        diffPane.resizeHandle = handle;
                        diffPane.resizeStartPoint = screenPoint;
                        diffPane.resizeStartRect = diffPane.persistentRect;
                        mouse.accepted = true;
                    } else if (isPointInScreenRect(screenPoint, screenRect)) {
                        diffPane.isDraggingPersistentRect = true;
                        var videoPoint2 = convertScreenToPixelCoordinates(screenPoint);
                        diffPane.dragStartOffset = Qt.point(videoPoint2.x - diffPane.persistentRect.x, videoPoint2.y - diffPane.persistentRect.y);
                        mouse.accepted = true;
                    } else {
                        mouse.accepted = false;
                    }
                } else {
                    mouse.accepted = false;
                }
            }
            onPositionChanged: function (mouse) {
                if (!diffPane.isPersistentRectSelecting && !diffPane.isDraggingPersistentRect && !diffPane.isResizingRect) {
                    // Keep Ctrl state in sync while idle
                    diffPane.isCtrlPressed = (mouse.modifiers & Qt.ControlModifier) !== 0;
                }
                if (diffPane.isPersistentRectSelecting) {
                    var currentVideoPos = convertScreenToPixelCoordinates(Qt.point(mouse.x, mouse.y));
                    var rect = Qt.rect(
                        Math.min(diffPane.persistentRectStart.x, currentVideoPos.x),
                        Math.min(diffPane.persistentRectStart.y, currentVideoPos.y),
                        Math.abs(currentVideoPos.x - diffPane.persistentRectStart.x),
                        Math.abs(currentVideoPos.y - diffPane.persistentRectStart.y)
                    );
                    diffPane.persistentRectEnd = Qt.point(rect.x + rect.width, rect.y + rect.height);
                    persistentRectCanvas.requestPaint();
                } else if (diffPane.isDraggingPersistentRect) {
                    var currentVideoPos2 = convertScreenToPixelCoordinates(Qt.point(mouse.x, mouse.y));
                    var newX = currentVideoPos2.x - diffPane.dragStartOffset.x;
                    var newY = currentVideoPos2.y - diffPane.dragStartOffset.y;
                    diffPane.persistentRect.x = newX;
                    diffPane.persistentRect.y = newY;
                    persistentRectCanvas.requestPaint();
                } else if (diffPane.isResizingRect) {
                    var currentVideoPos3 = convertScreenToPixelCoordinates(Qt.point(mouse.x, mouse.y));
                    var startVideoPos = convertScreenToPixelCoordinates(diffPane.resizeStartPoint);
                    var deltaX = currentVideoPos3.x - startVideoPos.x;
                    var deltaY = currentVideoPos3.y - startVideoPos.y;
                    var newRect = Qt.rect(diffPane.resizeStartRect.x, diffPane.resizeStartRect.y, diffPane.resizeStartRect.width, diffPane.resizeStartRect.height);
                    switch (diffPane.resizeHandle) {
                        case "nw":
                            newRect.x = Math.min(diffPane.resizeStartRect.x + deltaX, diffPane.resizeStartRect.x + diffPane.resizeStartRect.width - 10);
                            newRect.y = Math.min(diffPane.resizeStartRect.y + deltaY, diffPane.resizeStartRect.y + diffPane.resizeStartRect.height - 10);
                            newRect.width = diffPane.resizeStartRect.width - (newRect.x - diffPane.resizeStartRect.x);
                            newRect.height = diffPane.resizeStartRect.height - (newRect.y - diffPane.resizeStartRect.y);
                            break;
                        case "ne":
                            newRect.y = Math.min(diffPane.resizeStartRect.y + deltaY, diffPane.resizeStartRect.y + diffPane.resizeStartRect.height - 10);
                            newRect.width = Math.max(10, diffPane.resizeStartRect.width + deltaX);
                            newRect.height = diffPane.resizeStartRect.height - (newRect.y - diffPane.resizeStartRect.y);
                            break;
                        case "sw":
                            newRect.x = Math.min(diffPane.resizeStartRect.x + deltaX, diffPane.resizeStartRect.x + diffPane.resizeStartRect.width - 10);
                            newRect.width = diffPane.resizeStartRect.width - (newRect.x - diffPane.resizeStartRect.x);
                            newRect.height = Math.max(10, diffPane.resizeStartRect.height + deltaY);
                            break;
                        case "se":
                            newRect.width = Math.max(10, diffPane.resizeStartRect.width + deltaX);
                            newRect.height = Math.max(10, diffPane.resizeStartRect.height + deltaY);
                            break;
                        case "n":
                            newRect.y = Math.min(diffPane.resizeStartRect.y + deltaY, diffPane.resizeStartRect.y + diffPane.resizeStartRect.height - 10);
                            newRect.height = diffPane.resizeStartRect.height - (newRect.y - diffPane.resizeStartRect.y);
                            break;
                        case "s":
                            newRect.height = Math.max(10, diffPane.resizeStartRect.height + deltaY);
                            break;
                        case "w":
                            newRect.x = Math.min(diffPane.resizeStartRect.x + deltaX, diffPane.resizeStartRect.x + diffPane.resizeStartRect.width - 10);
                            newRect.width = diffPane.resizeStartRect.width - (newRect.x - diffPane.resizeStartRect.x);
                            break;
                        case "e":
                            newRect.width = Math.max(10, diffPane.resizeStartRect.width + deltaX);
                            break;
                    }
                    diffPane.persistentRect = newRect;
                    persistentRectCanvas.requestPaint();
                }
            }
            onReleased: function (mouse) {
                if (diffPane.isPersistentRectSelecting) {
                    diffPane.isPersistentRectSelecting = false;
                    var rect = Qt.rect(
                        Math.min(diffPane.persistentRectStart.x, diffPane.persistentRectEnd.x),
                        Math.min(diffPane.persistentRectStart.y, diffPane.persistentRectEnd.y),
                        Math.abs(diffPane.persistentRectEnd.x - diffPane.persistentRectStart.x),
                        Math.abs(diffPane.persistentRectEnd.y - diffPane.persistentRectStart.y)
                    );
                    if (rect.width > 5 && rect.height > 5) {
                        diffPane.hasPersistentRect = true;
                        diffPane.persistentRect = rect;
                        persistentRectCanvas.requestPaint();
                    }
                    diffPane.persistentRectStart = Qt.point(0, 0);
                    diffPane.persistentRectEnd = Qt.point(0, 0);
                    if (!diffPane.hasPersistentRect) {
                        persistentRectCanvas.requestPaint();
                    }
                } else if (diffPane.isDraggingPersistentRect) {
                    diffPane.isDraggingPersistentRect = false;
                    diffPane.dragStartOffset = Qt.point(0, 0);
                } else if (diffPane.isResizingRect) {
                    diffPane.isResizingRect = false;
                    diffPane.resizeHandle = "";
                    diffPane.resizeStartPoint = Qt.point(0, 0);
                    diffPane.resizeStartRect = Qt.rect(0, 0, 0, 0);
                }
                // Update Ctrl state from event to ensure cursor resets properly on Windows
                diffPane.isCtrlPressed = (mouse.modifiers & Qt.ControlModifier) !== 0;
            }

            onDoubleClicked: function (mouse) {
                if (diffPane.hasPersistentRect) {
                    // End any ongoing drag/resize/select to ensure double-click is handled
                    diffPane.isPersistentRectSelecting = false;
                    diffPane.isDraggingPersistentRect = false;
                    diffPane.isResizingRect = false;

                    var screenPoint = Qt.point(mouse.x, mouse.y);
                    var screenRect = convertVideoToScreenCoordinates(diffPane.persistentRect);
                    if (isPointInScreenRect(screenPoint, screenRect)) {
                        // Zoom to the rectangle (convert to screen coordinates for sharedView)
                        var zoomRect = convertVideoToScreenCoordinates(diffPane.persistentRect);
                        if (diffVideoWindow.sharedView) {
                            diffVideoWindow.sharedView.zoomToSelection(
                                zoomRect,
                                pixelValuesCanvas.getVideoRect(),
                                diffVideoWindow.sharedView.zoom,
                                diffVideoWindow.sharedView.centerX,
                                diffVideoWindow.sharedView.centerY
                            );
                        }
                        // Clear the persistent rectangle after zoom
                        removePersistentRect();
                    }
                }
            }
        }

        // Keep rectangle synced with view changes and frames
        Connections {
            target: diffVideoWindow.sharedView
            enabled: diffVideoWindow.sharedView !== null
            function onViewChanged() {
                if (persistentRectCanvas)
                    persistentRectCanvas.requestPaint();
            }
        }
        Connections {
            target: diffVideoWindow
            function onFrameReady() {
                if (persistentRectCanvas)
                    persistentRectCanvas.requestPaint();
            }
        }

        // Draw persistent rectangle and creation overlay
        Canvas {
            id: persistentRectCanvas
            anchors.fill: parent
            z: 3
            onPaint: {
                var ctx = getContext("2d");
                ctx.clearRect(0, 0, width, height);
                // Draw persistent rectangle if it exists
                if (diffPane.hasPersistentRect) {
                    var screenRect = convertVideoToScreenCoordinates(diffPane.persistentRect);
                    ctx.fillStyle = "rgba(255, 0, 0, 0.1)";
                    ctx.fillRect(screenRect.x, screenRect.y, screenRect.width, screenRect.height);
                    ctx.strokeStyle = "red";
                    ctx.lineWidth = 2;
                    ctx.strokeRect(screenRect.x, screenRect.y, screenRect.width, screenRect.height);
                    drawResizeHandles(ctx, screenRect);
                    var coordText = diffPane.persistentRect.width + ":" + diffPane.persistentRect.height + ":" + diffPane.persistentRect.x + ":" + diffPane.persistentRect.y;
                    drawCoordinateText(ctx, screenRect, coordText);
                }
                // Draw during creation (dashed)
                if (diffPane.isPersistentRectSelecting) {
                    var rect = Qt.rect(
                        Math.min(diffPane.persistentRectStart.x, diffPane.persistentRectEnd.x),
                        Math.min(diffPane.persistentRectStart.y, diffPane.persistentRectEnd.y),
                        Math.abs(diffPane.persistentRectEnd.x - diffPane.persistentRectStart.x),
                        Math.abs(diffPane.persistentRectEnd.y - diffPane.persistentRectStart.y)
                    );
                    var screenRect2 = convertVideoToScreenCoordinates(rect);
                    ctx.fillStyle = "rgba(255, 0, 0, 0.2)";
                    ctx.fillRect(screenRect2.x, screenRect2.y, screenRect2.width, screenRect2.height);
                    ctx.strokeStyle = "red";
                    ctx.lineWidth = 2;
                    ctx.setLineDash([5, 5]);
                    ctx.strokeRect(screenRect2.x, screenRect2.y, screenRect2.width, screenRect2.height);
                    ctx.setLineDash([]);
                    var coordText2 = rect.width + ":" + rect.height + ":" + rect.x + ":" + rect.y;
                    drawCoordinateText(ctx, screenRect2, coordText2);
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
                    ctx.font = Math.floor(fontSize) + "px " + Theme.fontFamily;

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
                    ctx.font = "12px" + Theme.fontFamily;
                } catch (error) {
                    console.log("QML: drawPixelValue error:", error);
                }
            }
        }

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
            z: 100
            Text {
                id: osdText
                anchors.centerIn: parent
                color: "white"
                font.family: Theme.fontFamily
                font.pixelSize: 12
                text: {
                    var s = diffPane.psnrInfo;
                    if (diffVideoWindow.osdState === 1)
                        return s;
                    if (diffVideoWindow.osdState === 2)
                        return s;
                    return "";
                }
            }
        }
    }

    function removePersistentRect() {
        hasPersistentRect = false;
        persistentRect = Qt.rect(0, 0, 0, 0);
        isPersistentRectSelecting = false;
        persistentRectCanvas.requestPaint();
    }

    // Convert video pixel coordinates to screen coordinates
    function convertVideoToScreenCoordinates(videoRect) {
        var meta = diffVideoWindow.getFrameMeta();
        if (!meta || !meta.yWidth || !meta.yHeight)
            return Qt.rect(0, 0, 0, 0);
        var screenRect = pixelValuesCanvas.getVideoRect();
        var normalizedX = videoRect.x / meta.yWidth;
        var normalizedY = videoRect.y / meta.yHeight;
        var normalizedWidth = videoRect.width / meta.yWidth;
        var normalizedHeight = videoRect.height / meta.yHeight;
        if (diffVideoWindow.sharedView) {
            normalizedX = (normalizedX - diffVideoWindow.sharedView.centerX) * diffVideoWindow.sharedView.zoom + 0.5;
            normalizedY = (normalizedY - diffVideoWindow.sharedView.centerY) * diffVideoWindow.sharedView.zoom + 0.5;
            normalizedWidth *= diffVideoWindow.sharedView.zoom;
            normalizedHeight *= diffVideoWindow.sharedView.zoom;
        }
        return Qt.rect(
            screenRect.x + normalizedX * screenRect.width,
            screenRect.y + normalizedY * screenRect.height,
            normalizedWidth * screenRect.width,
            normalizedHeight * screenRect.height
        );
    }

    // Convert screen coordinates to video pixel coordinates
    function convertScreenToPixelCoordinates(screenPoint) {
        var meta = diffVideoWindow.getFrameMeta();
        if (!meta || !meta.yWidth || !meta.yHeight)
            return Qt.point(0, 0);
        var videoRect = pixelValuesCanvas.getVideoRect();
        var normalizedX = (screenPoint.x - videoRect.x) / videoRect.width;
        var normalizedY = (screenPoint.y - videoRect.y) / videoRect.height;
        if (diffVideoWindow.sharedView) {
            normalizedX = (normalizedX - 0.5) / diffVideoWindow.sharedView.zoom + diffVideoWindow.sharedView.centerX;
            normalizedY = (normalizedY - 0.5) / diffVideoWindow.sharedView.zoom + diffVideoWindow.sharedView.centerY;
        }
        var pixelX = Math.round(normalizedX * meta.yWidth);
        var pixelY = Math.round(normalizedY * meta.yHeight);
        return Qt.point(pixelX, pixelY);
    }

    function drawCoordinateText(ctx, screenRect, coordText) {
        try {
            ctx.font = "12px" + Theme.fontFamily;
            ctx.textAlign = "left";
            ctx.textBaseline = "top";
            var textX = screenRect.x + 5;
            var textY = screenRect.y + 5;
            var textWidth = ctx.measureText(coordText).width;
            var textHeight = 14;
            ctx.fillStyle = "rgba(0, 0, 0, 0.7)";
            ctx.fillRect(textX - 2, textY - 2, textWidth + 4, textHeight + 4);
            ctx.fillStyle = "white";
            ctx.fillText(coordText, textX, textY);
        } catch (error) {
            console.log("QML: drawCoordinateText error:", error);
        }
    }

    function drawResizeHandles(ctx, screenRect) {
        try {
            var handleSize = 6;
            var handleColor = "red";
            var handleBorderColor = "red";
            ctx.fillStyle = handleColor;
            ctx.fillRect(screenRect.x - handleSize/2, screenRect.y - handleSize/2, handleSize, handleSize);
            ctx.strokeStyle = handleBorderColor;
            ctx.lineWidth = 1;
            ctx.strokeRect(screenRect.x - handleSize/2, screenRect.y - handleSize/2, handleSize, handleSize);
            ctx.fillStyle = handleColor;
            ctx.fillRect(screenRect.x + screenRect.width - handleSize/2, screenRect.y - handleSize/2, handleSize, handleSize);
            ctx.strokeStyle = handleBorderColor;
            ctx.lineWidth = 1;
            ctx.strokeRect(screenRect.x + screenRect.width - handleSize/2, screenRect.y - handleSize/2, handleSize, handleSize);
            ctx.fillStyle = handleColor;
            ctx.fillRect(screenRect.x - handleSize/2, screenRect.y + screenRect.height - handleSize/2, handleSize, handleSize);
            ctx.strokeStyle = handleBorderColor;
            ctx.lineWidth = 1;
            ctx.strokeRect(screenRect.x - handleSize/2, screenRect.y + screenRect.height - handleSize/2, handleSize, handleSize);
            ctx.fillStyle = handleColor;
            ctx.fillRect(screenRect.x + screenRect.width - handleSize/2, screenRect.y + screenRect.height - handleSize/2, handleSize, handleSize);
            ctx.strokeStyle = handleBorderColor;
            ctx.lineWidth = 1;
            ctx.strokeRect(screenRect.x + screenRect.width - handleSize/2, screenRect.y + screenRect.height - handleSize/2, handleSize, handleSize);
            ctx.fillStyle = handleColor;
            ctx.fillRect(screenRect.x + screenRect.width/2 - handleSize/2, screenRect.y - handleSize/2, handleSize, handleSize);
            ctx.strokeStyle = handleBorderColor;
            ctx.lineWidth = 1;
            ctx.strokeRect(screenRect.x + screenRect.width/2 - handleSize/2, screenRect.y - handleSize/2, handleSize, handleSize);
            ctx.fillStyle = handleColor;
            ctx.fillRect(screenRect.x + screenRect.width/2 - handleSize/2, screenRect.y + screenRect.height - handleSize/2, handleSize, handleSize);
            ctx.strokeStyle = handleBorderColor;
            ctx.lineWidth = 1;
            ctx.strokeRect(screenRect.x + screenRect.width/2 - handleSize/2, screenRect.y + screenRect.height - handleSize/2, handleSize, handleSize);
            ctx.fillStyle = handleColor;
            ctx.fillRect(screenRect.x - handleSize/2, screenRect.y + screenRect.height/2 - handleSize/2, handleSize, handleSize);
            ctx.strokeStyle = handleBorderColor;
            ctx.lineWidth = 1;
            ctx.strokeRect(screenRect.x - handleSize/2, screenRect.y + screenRect.height/2 - handleSize/2, handleSize, handleSize);
            ctx.fillStyle = handleColor;
            ctx.fillRect(screenRect.x + screenRect.width - handleSize/2, screenRect.y + screenRect.height/2 - handleSize/2, handleSize, handleSize);
            ctx.strokeStyle = handleBorderColor;
            ctx.lineWidth = 1;
            ctx.strokeRect(screenRect.x + screenRect.width - handleSize/2, screenRect.y + screenRect.height/2 - handleSize/2, handleSize, handleSize);
        } catch (error) {
            console.log("QML: drawResizeHandles error:", error);
        }
    }

    function isPointInScreenRect(point, rect) {
        return point.x >= rect.x && point.x <= rect.x + rect.width &&
               point.y >= rect.y && point.y <= rect.y + rect.height;
    }

    function getResizeHandleAtPoint(point, rect) {
        var handleSize = 8;
        var x = point.x;
        var y = point.y;
        var left = rect.x;
        var right = rect.x + rect.width;
        var top = rect.y;
        var bottom = rect.y + rect.height;
        if (x >= left - handleSize && x <= left + handleSize && y >= top - handleSize && y <= top + handleSize) {
            return "nw";
        }
        if (x >= right - handleSize && x <= right + handleSize && y >= top - handleSize && y <= top + handleSize) {
            return "ne";
        }
        if (x >= left - handleSize && x <= left + handleSize && y >= bottom - handleSize && y <= bottom + handleSize) {
            return "sw";
        }
        if (x >= right - handleSize && x <= right + handleSize && y >= bottom - handleSize && y <= bottom + handleSize) {
            return "se";
        }
        if (x >= left - handleSize && x <= left + handleSize && y >= top && y <= bottom) {
            return "w";
        }
        if (x >= right - handleSize && x <= right + handleSize && y >= top && y <= bottom) {
            return "e";
        }
        if (y >= top - handleSize && y <= top + handleSize && x >= left && x <= right) {
            return "n";
        }
        if (y >= bottom - handleSize && y <= bottom + handleSize && x >= left && x <= right) {
            return "s";
        }
        return "";
    }
}
