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
    property string resizeHandle: "" // "nw", "ne", "sw", "se", "n", "s", "w", "e"
    property point resizeStartPoint: Qt.point(0, 0)
    property rect resizeStartRect: Qt.rect(0, 0, 0, 0)
    property bool isMouseDown: false
    property bool isZoomed: false
    property bool isCtrlPressed: mainWindow ? mainWindow.isCtrlPressed : false
    property bool resizing: false

    onWidthChanged: {
        if (!resizing) {
            resizing = true;
            wasPlayingBeforeResize = videoController.isPlaying;
            if (wasPlayingBeforeResize) {
                videoController.pause();
            }
        }
        resizeDebounce.restart();
    }

    onHeightChanged: {
        if (!resizing) {
            resizing = true;
            wasPlayingBeforeResize = videoController.isPlaying;
            if (wasPlayingBeforeResize) {
                videoController.pause();
            }
        }
        resizeDebounce.restart();
    }

    onXChanged: {
        if (!resizing) {
            resizing = true;
            wasPlayingBeforeResize = videoController.isPlaying;
            if (wasPlayingBeforeResize) {
                videoController.pause();
            }
        }
        resizeDebounce.restart();
    }

    onYChanged: {
        if (!resizing) {
            resizing = true;
            wasPlayingBeforeResize = videoController.isPlaying;
            if (wasPlayingBeforeResize) {
                videoController.pause();
            }
        }
        resizeDebounce.restart();
    }

    Timer {
        id: resizeDebounce
        interval: 200
        repeat: false
        onTriggered: {
            resizing = false;
            if (wasPlayingBeforeResize) {
                videoController.play();
            }
        }
    }

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
                    // Clear existing rectangle when Ctrl is pressed
                    if (mainWindow.isCtrlPressed) {
                        // Ensure global single-rectangle policy: clear own rectangle
                        if (diffWindow.hasPersistentRect) {
                            removePersistentRect();
                        }
                        // Also request main window to clear all rectangles elsewhere
                        if (mainWindow.clearAllRectangles)
                            mainWindow.clearAllRectangles();
                    }
                }
            }
        }

        // Listen for view changes to redraw rectangle
        Connections {
            target: diffVideoWindow.sharedView
            enabled: diffVideoWindow.sharedView !== null
            function onViewChanged() {
                // Redraw persistent rectangle when view changes
                if (diffWindow.hasPersistentRect) {
                    persistentRectCanvas.requestPaint();
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

                    MenuSeparator {}

                    MenuItem {
                        visible: mainWindow && !mainWindow.isEmbeddedDiffActive()
                        text: "Embed Diff Window"
                        onTriggered: {
                            mainWindow.closingDiffPopupForEmbed = true;
                            mainWindow.enableEmbeddedDiff();
                            diffWindow.close();
                        }
                    }

                    MenuSeparator {}

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
                // Show resize cursors when hovering over handles
                if (diffWindow && diffWindow.hasPersistentRect) {
                    var screenRect = diffWindow.convertVideoToScreenCoordinates(diffWindow.persistentRect);
                    var handle = diffWindow.getResizeHandleAtPoint(Qt.point(mouseX, mouseY), screenRect);
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
                if (diffWindow && diffWindow.isCtrlPressed)
                    return Qt.CrossCursor;
                if (diffWindow && diffWindow.isZoomed) {
                    return (diffWindow.isMouseDown) ? Qt.ClosedHandCursor : Qt.OpenHandCursor;
                }
                return Qt.ArrowCursor;
            }

            onPressed: function (mouse) {
                // Determine Ctrl state from the actual event to avoid stale global state
                var ctrlDown = (mouse.modifiers & Qt.ControlModifier) !== 0;
                if (diffWindow) {
                    diffWindow.isCtrlPressed = ctrlDown;
                }
                if (diffWindow && ctrlDown) {
                    // If an existing persistent rectangle is present, clear it before starting a new selection
                    if (diffWindow.hasPersistentRect) {
                        diffWindow.removePersistentRect();
                    }
                    // Start creating new persistent rectangle
                    diffWindow.isPersistentRectSelecting = true;
                    // Convert screen coordinates to video pixel coordinates for start point
                    var videoPoint = diffWindow.convertScreenToPixelCoordinates(Qt.point(mouse.x, mouse.y));
                    diffWindow.persistentRectStart = videoPoint;
                    diffWindow.persistentRectEnd = videoPoint;
                } else if (diffWindow && diffWindow.hasPersistentRect && !diffWindow.isPersistentRectSelecting) {
                    // Check if mouse is on resize handle or inside the persistent rectangle
                    var screenPoint = Qt.point(mouse.x, mouse.y);
                    var screenRect = diffWindow.convertVideoToScreenCoordinates(diffWindow.persistentRect);
                    var handle = diffWindow.getResizeHandleAtPoint(screenPoint, screenRect);
                    
                    if (handle !== "") {
                        // Start resizing the rectangle
                        diffWindow.isResizingRect = true;
                        diffWindow.resizeHandle = handle;
                        diffWindow.resizeStartPoint = screenPoint;
                        diffWindow.resizeStartRect = diffWindow.persistentRect;
                        mouse.accepted = true;
                    } else if (diffWindow.isPointInScreenRect(screenPoint, screenRect)) {
                        // Start dragging the rectangle
                        diffWindow.isDraggingPersistentRect = true;
                        var videoPoint = diffWindow.convertScreenToPixelCoordinates(screenPoint);
                        diffWindow.dragStartOffset = Qt.point(videoPoint.x - diffWindow.persistentRect.x, videoPoint.y - diffWindow.persistentRect.y);
                        mouse.accepted = true;
                    } else {
                        mouse.accepted = false;
                    }
                } else {
                    mouse.accepted = false;
                }
            }

            onPositionChanged: function (mouse) {
                // Keep local Ctrl state in sync when not actively selecting/dragging/resizing
                if (diffWindow && !diffWindow.isPersistentRectSelecting && !diffWindow.isDraggingPersistentRect && !diffWindow.isResizingRect) {
                    diffWindow.isCtrlPressed = (mouse.modifiers & Qt.ControlModifier) !== 0;
                }
                if (diffWindow && diffWindow.isPersistentRectSelecting) {
                    // Convert current mouse position to video pixel coordinates
                    var currentVideoPos = diffWindow.convertScreenToPixelCoordinates(Qt.point(mouse.x, mouse.y));
                    
                    // Calculate rectangle in video pixel coordinates
                    var rect = Qt.rect(
                        Math.min(diffWindow.persistentRectStart.x, currentVideoPos.x),
                        Math.min(diffWindow.persistentRectStart.y, currentVideoPos.y),
                        Math.abs(currentVideoPos.x - diffWindow.persistentRectStart.x),
                        Math.abs(currentVideoPos.y - diffWindow.persistentRectStart.y)
                    );
                    
                    diffWindow.persistentRectEnd = Qt.point(rect.x + rect.width, rect.y + rect.height);
                    persistentRectCanvas.requestPaint();
                } else if (diffWindow && diffWindow.isDraggingPersistentRect) {
                    // Drag the persistent rectangle
                    var currentVideoPos = diffWindow.convertScreenToPixelCoordinates(Qt.point(mouse.x, mouse.y));
                    var newX = currentVideoPos.x - diffWindow.dragStartOffset.x;
                    var newY = currentVideoPos.y - diffWindow.dragStartOffset.y;
                    
                    // Update rectangle position
                    diffWindow.persistentRect.x = newX;
                    diffWindow.persistentRect.y = newY;
                    persistentRectCanvas.requestPaint();
                } else if (diffWindow && diffWindow.isResizingRect) {
                    // Resize the persistent rectangle
                    var currentVideoPos = diffWindow.convertScreenToPixelCoordinates(Qt.point(mouse.x, mouse.y));
                    var startVideoPos = diffWindow.convertScreenToPixelCoordinates(diffWindow.resizeStartPoint);
                    var deltaX = currentVideoPos.x - startVideoPos.x;
                    var deltaY = currentVideoPos.y - startVideoPos.y;
                    
                    var newRect = Qt.rect(diffWindow.resizeStartRect.x, diffWindow.resizeStartRect.y, diffWindow.resizeStartRect.width, diffWindow.resizeStartRect.height);
                    
                    // Apply resize based on handle
                    switch (diffWindow.resizeHandle) {
                        case "nw":
                            newRect.x = Math.min(diffWindow.resizeStartRect.x + deltaX, diffWindow.resizeStartRect.x + diffWindow.resizeStartRect.width - 10);
                            newRect.y = Math.min(diffWindow.resizeStartRect.y + deltaY, diffWindow.resizeStartRect.y + diffWindow.resizeStartRect.height - 10);
                            newRect.width = diffWindow.resizeStartRect.width - (newRect.x - diffWindow.resizeStartRect.x);
                            newRect.height = diffWindow.resizeStartRect.height - (newRect.y - diffWindow.resizeStartRect.y);
                            break;
                        case "ne":
                            newRect.y = Math.min(diffWindow.resizeStartRect.y + deltaY, diffWindow.resizeStartRect.y + diffWindow.resizeStartRect.height - 10);
                            newRect.width = Math.max(10, diffWindow.resizeStartRect.width + deltaX);
                            newRect.height = diffWindow.resizeStartRect.height - (newRect.y - diffWindow.resizeStartRect.y);
                            break;
                        case "sw":
                            newRect.x = Math.min(diffWindow.resizeStartRect.x + deltaX, diffWindow.resizeStartRect.x + diffWindow.resizeStartRect.width - 10);
                            newRect.width = diffWindow.resizeStartRect.width - (newRect.x - diffWindow.resizeStartRect.x);
                            newRect.height = Math.max(10, diffWindow.resizeStartRect.height + deltaY);
                            break;
                        case "se":
                            newRect.width = Math.max(10, diffWindow.resizeStartRect.width + deltaX);
                            newRect.height = Math.max(10, diffWindow.resizeStartRect.height + deltaY);
                            break;
                        case "n":
                            newRect.y = Math.min(diffWindow.resizeStartRect.y + deltaY, diffWindow.resizeStartRect.y + diffWindow.resizeStartRect.height - 10);
                            newRect.height = diffWindow.resizeStartRect.height - (newRect.y - diffWindow.resizeStartRect.y);
                            break;
                        case "s":
                            newRect.height = Math.max(10, diffWindow.resizeStartRect.height + deltaY);
                            break;
                        case "w":
                            newRect.x = Math.min(diffWindow.resizeStartRect.x + deltaX, diffWindow.resizeStartRect.x + diffWindow.resizeStartRect.width - 10);
                            newRect.width = diffWindow.resizeStartRect.width - (newRect.x - diffWindow.resizeStartRect.x);
                            break;
                        case "e":
                            newRect.width = Math.max(10, diffWindow.resizeStartRect.width + deltaX);
                            break;
                    }
                    
                    // Update rectangle
                    diffWindow.persistentRect = newRect;
                    persistentRectCanvas.requestPaint();
                }
            }

            onReleased: function (mouse) {
                if (diffWindow && diffWindow.isPersistentRectSelecting) {
                    diffWindow.isPersistentRectSelecting = false;
                    
                    // Calculate final persistent rectangle in video pixel coordinates
                    var rect = Qt.rect(
                        Math.min(diffWindow.persistentRectStart.x, diffWindow.persistentRectEnd.x),
                        Math.min(diffWindow.persistentRectStart.y, diffWindow.persistentRectEnd.y),
                        Math.abs(diffWindow.persistentRectEnd.x - diffWindow.persistentRectStart.x),
                        Math.abs(diffWindow.persistentRectEnd.y - diffWindow.persistentRectStart.y)
                    );
                    
                    // Only create rectangle if it has meaningful size
                    if (rect.width > 5 && rect.height > 5) {
                        diffWindow.hasPersistentRect = true;
                        diffWindow.persistentRect = rect;
                        persistentRectCanvas.requestPaint();
                    }
                    
                    // Clear creation state
                    diffWindow.persistentRectStart = Qt.point(0, 0);
                    diffWindow.persistentRectEnd = Qt.point(0, 0);
                    // Ensure any dashed selection is cleared if rectangle wasn't created
                    if (!diffWindow.hasPersistentRect) {
                        persistentRectCanvas.requestPaint();
                    }
                } else if (diffWindow && diffWindow.isDraggingPersistentRect) {
                    // End dragging the persistent rectangle
                    diffWindow.isDraggingPersistentRect = false;
                    diffWindow.dragStartOffset = Qt.point(0, 0);
                } else if (diffWindow && diffWindow.isResizingRect) {
                    // End resizing the persistent rectangle
                    diffWindow.isResizingRect = false;
                    diffWindow.resizeHandle = "";
                    diffWindow.resizeStartPoint = Qt.point(0, 0);
                    diffWindow.resizeStartRect = Qt.rect(0, 0, 0, 0);
                }
                // After mouse release, update Ctrl state from event to ensure cursor resets properly on Windows
                if (diffWindow) {
                    diffWindow.isCtrlPressed = (mouse.modifiers & Qt.ControlModifier) !== 0;
                }
            }

            onDoubleClicked: function (mouse) {
                if (diffWindow && diffWindow.hasPersistentRect) {
                    // End any ongoing drag/resize/select to ensure double-click is handled
                    diffWindow.isPersistentRectSelecting = false;
                    diffWindow.isDraggingPersistentRect = false;
                    diffWindow.isResizingRect = false;
                    // Check if double-click is inside the persistent rectangle
                    var screenPoint = Qt.point(mouse.x, mouse.y);
                    var screenRect = diffWindow.convertVideoToScreenCoordinates(diffWindow.persistentRect);
                    
                    if (diffWindow.isPointInScreenRect(screenPoint, screenRect)) {
                        // Convert rectangle to screen coordinates for zoom
                        var rect = diffWindow.convertVideoToScreenCoordinates(diffWindow.persistentRect);
                        
                        // Zoom to the rectangle
                        if (diffVideoWindow.sharedView) {
                            diffVideoWindow.sharedView.zoomToSelection(rect, pixelValuesCanvas.getVideoRect(), 
                                                                     diffVideoWindow.sharedView.zoom, 
                                                                     diffVideoWindow.sharedView.centerX, 
                                                                     diffVideoWindow.sharedView.centerY);
                        }
                        
                        // Clear the persistent rectangle
                        diffWindow.removePersistentRect();
                    }
                }
            }
        }

        // Draw persistent rectangle
        Canvas {
            id: persistentRectCanvas
            anchors.fill: parent
            z: 3

            onPaint: {
                var ctx = getContext("2d");
                ctx.clearRect(0, 0, width, height);
                
                // Draw persistent rectangle if it exists
                if (diffWindow && diffWindow.hasPersistentRect) {
                    // Convert video pixel coordinates to screen coordinates for drawing
                    var screenRect = diffWindow.convertVideoToScreenCoordinates(diffWindow.persistentRect);
                    ctx.fillStyle = "rgba(255, 0, 0, 0.1)";
                    ctx.fillRect(screenRect.x, screenRect.y, screenRect.width, screenRect.height);
                    ctx.strokeStyle = "red";
                    ctx.lineWidth = 2;
                    ctx.strokeRect(screenRect.x, screenRect.y, screenRect.width, screenRect.height);
                    
                    // Draw resize handles
                    diffWindow.drawResizeHandles(ctx, screenRect);
                    
                    // Draw coordinate text
                    var coordText = "W" + diffWindow.persistentRect.width + ":" + "H" + diffWindow.persistentRect.height + ":" + "X" + diffWindow.persistentRect.x + ":" + "Y" + diffWindow.persistentRect.y;
                    diffWindow.drawCoordinateText(ctx, screenRect, coordText);
                }
                
                // Draw persistent rectangle during creation
                if (diffWindow && diffWindow.isPersistentRectSelecting) {
                    // Convert video pixel coordinates to screen coordinates for drawing
                    var rect = Qt.rect(
                        Math.min(diffWindow.persistentRectStart.x, diffWindow.persistentRectEnd.x),
                        Math.min(diffWindow.persistentRectStart.y, diffWindow.persistentRectEnd.y),
                        Math.abs(diffWindow.persistentRectEnd.x - diffWindow.persistentRectStart.x),
                        Math.abs(diffWindow.persistentRectEnd.y - diffWindow.persistentRectStart.y)
                    );
                    var screenRect = diffWindow.convertVideoToScreenCoordinates(rect);
                    ctx.fillStyle = "rgba(255, 0, 0, 0.2)";
                    ctx.fillRect(screenRect.x, screenRect.y, screenRect.width, screenRect.height);
                    ctx.strokeStyle = "red";
                    ctx.lineWidth = 2;
                    ctx.setLineDash([5, 5]);
                    ctx.strokeRect(screenRect.x, screenRect.y, screenRect.width, screenRect.height);
                    ctx.setLineDash([]);
                    
                    // Draw coordinate text during creation
                    var coordText = "W" + rect.width + ":" + "H" + rect.height + ":" + "X" + rect.x + ":" + "Y" + rect.y;
                    diffWindow.drawCoordinateText(ctx, screenRect, coordText);
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
                        console.log("[QML] Invalid bounds detected");
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
                                console.log("[QML] Error processing pixel at", x, y, ":", error);
                                continue;
                            }
                        }
                    }
                } catch (error) {
                    console.log("[QML] Canvas onPaint error:", error);
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
                    ctx.font = "12px " + Theme.fontFamily;
                } catch (error) {
                    console.log("[QML] drawPixelValue error:", error);
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
                font.family: Theme.fontFamily
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
        
        // Convert video pixel coordinates to normalized coordinates (0-1)
        var normalizedX = videoRect.x / meta.yWidth;
        var normalizedY = videoRect.y / meta.yHeight;
        var normalizedWidth = videoRect.width / meta.yWidth;
        var normalizedHeight = videoRect.height / meta.yHeight;
        
        // Apply zoom and pan transformation
        if (diffVideoWindow.sharedView) {
            normalizedX = (normalizedX - diffVideoWindow.sharedView.centerX) * diffVideoWindow.sharedView.zoom + 0.5;
            normalizedY = (normalizedY - diffVideoWindow.sharedView.centerY) * diffVideoWindow.sharedView.zoom + 0.5;
            normalizedWidth *= diffVideoWindow.sharedView.zoom;
            normalizedHeight *= diffVideoWindow.sharedView.zoom;
        }
        
        // Convert to screen coordinates
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
        
        // Convert screen coordinates to normalized video coordinates (0-1)
        var normalizedX = (screenPoint.x - videoRect.x) / videoRect.width;
        var normalizedY = (screenPoint.y - videoRect.y) / videoRect.height;
        
        // Apply inverse zoom and pan transformation to get actual video coordinates
        if (diffVideoWindow.sharedView) {
            // Inverse transformation: account for current view state
            normalizedX = (normalizedX - 0.5) / diffVideoWindow.sharedView.zoom + diffVideoWindow.sharedView.centerX;
            normalizedY = (normalizedY - 0.5) / diffVideoWindow.sharedView.zoom + diffVideoWindow.sharedView.centerY;
        }
        
        // Convert to actual pixel coordinates
        var pixelX = Math.round(normalizedX * meta.yWidth);
        var pixelY = Math.round(normalizedY * meta.yHeight);
        
        return Qt.point(pixelX, pixelY);
    }

    // Get current visible video bounds (pixel coordinates) from sharedView
    function getVisibleVideoBounds() {
        var meta = diffVideoWindow.getFrameMeta();
        if (!meta || !meta.yWidth || !meta.yHeight || !diffVideoWindow.sharedView)
            return Qt.rect(0, 0, 0, 0);

        var zoom = Math.max(1.0, diffVideoWindow.sharedView.zoom);
        var cx = diffVideoWindow.sharedView.centerX;
        var cy = diffVideoWindow.sharedView.centerY;

        // Normalized view rect in video space
        var viewWn = Math.min(1.0, 1.0 / zoom);
        var viewHn = Math.min(1.0, 1.0 / zoom);
        var viewXn = cx - viewWn / 2.0;
        var viewYn = cy - viewHn / 2.0;
        // Clamp to [0,1]
        viewXn = Math.max(0.0, Math.min(1.0 - viewWn, viewXn));
        viewYn = Math.max(0.0, Math.min(1.0 - viewHn, viewYn));

        // Convert to pixel coordinates
        var x = Math.round(viewXn * meta.yWidth);
        var y = Math.round(viewYn * meta.yHeight);
        var w = Math.round(viewWn * meta.yWidth);
        var h = Math.round(viewHn * meta.yHeight);

        // Final clamp to video edges
        x = Math.max(0, Math.min(meta.yWidth, x));
        y = Math.max(0, Math.min(meta.yHeight, y));
        w = Math.max(0, Math.min(meta.yWidth - x, w));
        h = Math.max(0, Math.min(meta.yHeight - y, h));

        return Qt.rect(x, y, w, h);
    }


    // Draw coordinate text on rectangle
    function drawCoordinateText(ctx, screenRect, coordText) {
        try {
            // Set font
            ctx.font = "12px " + Theme.fontFamily;
            ctx.textAlign = "left";
            ctx.textBaseline = "top";
            
            // Calculate text position (top-left corner of rectangle)
            var textX = screenRect.x + 5;
            var textY = screenRect.y + 5;
            
            // Measure text width
            var textWidth = ctx.measureText(coordText).width;
            var textHeight = 14; // Approximate height for 12px font
            
            // Draw background
            ctx.fillStyle = "rgba(0, 0, 0, 0.7)";
            ctx.fillRect(textX - 2, textY - 2, textWidth + 4, textHeight + 4);
            
            // Draw text
            ctx.fillStyle = "white";
            ctx.fillText(coordText, textX, textY);
        } catch (error) {
            console.log("[QML] drawCoordinateText error:", error);
        }
    }

    // Draw resize handles on rectangle
    function drawResizeHandles(ctx, screenRect) {
        try {
            var handleSize = 6;
            var handleColor = "red";
            var handleBorderColor = "red";
            
            // Draw corner handles
            // Top-left
            ctx.fillStyle = handleColor;
            ctx.fillRect(screenRect.x - handleSize/2, screenRect.y - handleSize/2, handleSize, handleSize);
            ctx.strokeStyle = handleBorderColor;
            ctx.lineWidth = 1;
            ctx.strokeRect(screenRect.x - handleSize/2, screenRect.y - handleSize/2, handleSize, handleSize);
            
            // Top-right
            ctx.fillStyle = handleColor;
            ctx.fillRect(screenRect.x + screenRect.width - handleSize/2, screenRect.y - handleSize/2, handleSize, handleSize);
            ctx.strokeStyle = handleBorderColor;
            ctx.lineWidth = 1;
            ctx.strokeRect(screenRect.x + screenRect.width - handleSize/2, screenRect.y - handleSize/2, handleSize, handleSize);
            
            // Bottom-left
            ctx.fillStyle = handleColor;
            ctx.fillRect(screenRect.x - handleSize/2, screenRect.y + screenRect.height - handleSize/2, handleSize, handleSize);
            ctx.strokeStyle = handleBorderColor;
            ctx.lineWidth = 1;
            ctx.strokeRect(screenRect.x - handleSize/2, screenRect.y + screenRect.height - handleSize/2, handleSize, handleSize);
            
            // Bottom-right
            ctx.fillStyle = handleColor;
            ctx.fillRect(screenRect.x + screenRect.width - handleSize/2, screenRect.y + screenRect.height - handleSize/2, handleSize, handleSize);
            ctx.strokeStyle = handleBorderColor;
            ctx.lineWidth = 1;
            ctx.strokeRect(screenRect.x + screenRect.width - handleSize/2, screenRect.y + screenRect.height - handleSize/2, handleSize, handleSize);
            
            // Draw edge handles
            // Top edge
            ctx.fillStyle = handleColor;
            ctx.fillRect(screenRect.x + screenRect.width/2 - handleSize/2, screenRect.y - handleSize/2, handleSize, handleSize);
            ctx.strokeStyle = handleBorderColor;
            ctx.lineWidth = 1;
            ctx.strokeRect(screenRect.x + screenRect.width/2 - handleSize/2, screenRect.y - handleSize/2, handleSize, handleSize);
            
            // Bottom edge
            ctx.fillStyle = handleColor;
            ctx.fillRect(screenRect.x + screenRect.width/2 - handleSize/2, screenRect.y + screenRect.height - handleSize/2, handleSize, handleSize);
            ctx.strokeStyle = handleBorderColor;
            ctx.lineWidth = 1;
            ctx.strokeRect(screenRect.x + screenRect.width/2 - handleSize/2, screenRect.y + screenRect.height - handleSize/2, handleSize, handleSize);
            
            // Left edge
            ctx.fillStyle = handleColor;
            ctx.fillRect(screenRect.x - handleSize/2, screenRect.y + screenRect.height/2 - handleSize/2, handleSize, handleSize);
            ctx.strokeStyle = handleBorderColor;
            ctx.lineWidth = 1;
            ctx.strokeRect(screenRect.x - handleSize/2, screenRect.y + screenRect.height/2 - handleSize/2, handleSize, handleSize);
            
            // Right edge
            ctx.fillStyle = handleColor;
            ctx.fillRect(screenRect.x + screenRect.width - handleSize/2, screenRect.y + screenRect.height/2 - handleSize/2, handleSize, handleSize);
            ctx.strokeStyle = handleBorderColor;
            ctx.lineWidth = 1;
            ctx.strokeRect(screenRect.x + screenRect.width - handleSize/2, screenRect.y + screenRect.height/2 - handleSize/2, handleSize, handleSize);
        } catch (error) {
            console.log("[QML] drawResizeHandles error:", error);
        }
    }

    // Check if a point is inside a screen rectangle
    function isPointInScreenRect(point, rect) {
        return point.x >= rect.x && point.x <= rect.x + rect.width &&
               point.y >= rect.y && point.y <= rect.y + rect.height;
    }

    // Get resize handle at a point
    function getResizeHandleAtPoint(point, rect) {
        var handleSize = 8;
        var x = point.x;
        var y = point.y;
        var left = rect.x;
        var right = rect.x + rect.width;
        var top = rect.y;
        var bottom = rect.y + rect.height;
        
        // Check corners first
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
        
        // Check edges
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
