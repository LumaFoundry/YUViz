import QtQuick 6.0
import QtQuick.Controls.Basic 6.0
import VideoWindow 1.0
import QtQuick.Layouts 1.15

VideoWindow {
    id: videoWindow
    property int videoId: -1
    property string videoDisplayName: videoWindow.videoName || ""
    property bool metadataReady: false
    property bool assigned: false
    property bool isSelecting: false
    property point selectionStart: Qt.point(0, 0)
    property point selectionEnd: Qt.point(0, 0)
    property bool isProcessingSelection: false
    
    // Persistent rectangle state (stored in video pixel coordinates)
    property bool hasPersistentRect: false
    property rect persistentRect: Qt.rect(0, 0, 0, 0) // Stored in video pixel coordinates
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
    property bool isCtrlPressed: mainWindow.isCtrlPressed
    property bool isZoomed: sharedView ? sharedView.isZoomed : false
    property var sharedView: sharedViewProperties
    objectName: "videoWindow_" + videoId

    signal requestRemove(int videoId)

    // Listen for Ctrl key state changes
    Connections {
        target: mainWindow
        function onIsCtrlPressedChanged() {
            if (mainWindow.isCtrlPressed) {
                // Clear own rectangle
                if (hasPersistentRect) {
                    removePersistentRect();
                }
                // Also ensure all other rectangles cleared globally
                if (mainWindow.clearAllRectangles)
                    mainWindow.clearAllRectangles();
            }
        }
    }

    // Listen for view changes to redraw rectangle
    Connections {
        target: sharedViewProperties
        function onViewChanged() {
            // Redraw persistent rectangle when view changes
            if (hasPersistentRect) {
                persistentRectCanvas.requestPaint();
            }
        }
    }

    // Video Name Display
    Rectangle {
        visible: videoWindow.assigned && videoWindow.metadataReady
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 5
        width: videoNameText.contentWidth + 16
        height: videoNameText.height + 12
        color: "black"
        opacity: 0.8
        radius: 4
        z: 101 // On top of everything including OSD

        Text {
            id: videoNameText
            anchors.centerIn: parent
            color: "white"
            font.family: "Arial"
            font.pixelSize: 14
            font.weight: Font.Bold
            text: videoWindow.videoDisplayName
        }
    }

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

                MenuSeparator {}

                MenuItem {
                    contentItem: ComboBox {
                        id: componentDisplaySelector
                        objectName: "componentDisplaySelector"
                        model: ["RGB", "Y Only", "U Only", "V Only"]
                        width: 160
                        currentIndex: videoWindow.componentDisplayMode

                        onCurrentIndexChanged: {
                            videoWindow.componentDisplayMode = currentIndex;
                        }
                        onActivated: keyHandler.forceActiveFocus()
                    }
                }

                MenuSeparator {}

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
            // Show resize cursors when hovering over handles
            if (hasPersistentRect) {
                var screenRect = convertVideoToScreenCoordinates(persistentRect);
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
            if (mainWindow.isCtrlPressed)
                return Qt.CrossCursor;
            if (videoWindow.isZoomed) {
                return (videoWindow.isMouseDown) ? Qt.ClosedHandCursor : Qt.OpenHandCursor;
            }
            return Qt.ArrowCursor;
        }

        onPressed: function (mouse) {
            if (mainWindow.isCtrlPressed) {
                // If an existing persistent rectangle is present, clear it before starting a new selection
                if (hasPersistentRect) {
                    removePersistentRect();
                }
                // Start creating new persistent rectangle
                isPersistentRectSelecting = true;
                // Convert screen coordinates to video pixel coordinates for start point
                var videoPoint = convertScreenToPixelCoordinates(Qt.point(mouse.x, mouse.y));
                persistentRectStart = videoPoint;
                persistentRectEnd = videoPoint;
            } else if (hasPersistentRect && !isPersistentRectSelecting) {
                // Check if mouse is on resize handle or inside the persistent rectangle
                var screenPoint = Qt.point(mouse.x, mouse.y);
                var screenRect = convertVideoToScreenCoordinates(persistentRect);
                var handle = getResizeHandleAtPoint(screenPoint, screenRect);
                
                if (handle !== "") {
                    // Start resizing the rectangle
                    isResizingRect = true;
                    resizeHandle = handle;
                    resizeStartPoint = screenPoint;
                    resizeStartRect = persistentRect;
                    mouse.accepted = true;
                } else if (isPointInScreenRect(screenPoint, screenRect)) {
                    // Start dragging the rectangle
                    isDraggingPersistentRect = true;
                    var videoPoint = convertScreenToPixelCoordinates(screenPoint);
                    dragStartOffset = Qt.point(videoPoint.x - persistentRect.x, videoPoint.y - persistentRect.y);
                    mouse.accepted = true;
                } else {
                    mouse.accepted = false;
                }
            } else {
                mouse.accepted = false;
            }
        }

        onPositionChanged: function (mouse) {
            if (isPersistentRectSelecting) {
                // Convert current mouse position to video pixel coordinates
                var currentVideoPos = convertScreenToPixelCoordinates(Qt.point(mouse.x, mouse.y));
                
                // Calculate rectangle in video pixel coordinates
                var rect = Qt.rect(
                    Math.min(persistentRectStart.x, currentVideoPos.x),
                    Math.min(persistentRectStart.y, currentVideoPos.y),
                    Math.abs(currentVideoPos.x - persistentRectStart.x),
                    Math.abs(currentVideoPos.y - persistentRectStart.y)
                );
                
                // No clamping: allow rectangle outside any bounds
                
                persistentRectEnd = Qt.point(rect.x + rect.width, rect.y + rect.height);
                persistentRectCanvas.requestPaint();
            } else if (isDraggingPersistentRect) {
                // Drag the persistent rectangle
                var currentVideoPos = convertScreenToPixelCoordinates(Qt.point(mouse.x, mouse.y));
                var newX = currentVideoPos.x - dragStartOffset.x;
                var newY = currentVideoPos.y - dragStartOffset.y;
                
                // No clamping when dragging
                
                // Update rectangle position
                persistentRect.x = newX;
                persistentRect.y = newY;
                persistentRectCanvas.requestPaint();
            } else if (isResizingRect) {
                // Resize the persistent rectangle
                var currentVideoPos = convertScreenToPixelCoordinates(Qt.point(mouse.x, mouse.y));
                var startVideoPos = convertScreenToPixelCoordinates(resizeStartPoint);
                var deltaX = currentVideoPos.x - startVideoPos.x;
                var deltaY = currentVideoPos.y - startVideoPos.y;
                
                var newRect = Qt.rect(resizeStartRect.x, resizeStartRect.y, resizeStartRect.width, resizeStartRect.height);
                
                // Apply resize based on handle
                switch (resizeHandle) {
                    case "nw":
                        newRect.x = Math.min(resizeStartRect.x + deltaX, resizeStartRect.x + resizeStartRect.width - 10);
                        newRect.y = Math.min(resizeStartRect.y + deltaY, resizeStartRect.y + resizeStartRect.height - 10);
                        newRect.width = resizeStartRect.width - (newRect.x - resizeStartRect.x);
                        newRect.height = resizeStartRect.height - (newRect.y - resizeStartRect.y);
                        break;
                    case "ne":
                        newRect.y = Math.min(resizeStartRect.y + deltaY, resizeStartRect.y + resizeStartRect.height - 10);
                        newRect.width = Math.max(10, resizeStartRect.width + deltaX);
                        newRect.height = resizeStartRect.height - (newRect.y - resizeStartRect.y);
                        break;
                    case "sw":
                        newRect.x = Math.min(resizeStartRect.x + deltaX, resizeStartRect.x + resizeStartRect.width - 10);
                        newRect.width = resizeStartRect.width - (newRect.x - resizeStartRect.x);
                        newRect.height = Math.max(10, resizeStartRect.height + deltaY);
                        break;
                    case "se":
                        newRect.width = Math.max(10, resizeStartRect.width + deltaX);
                        newRect.height = Math.max(10, resizeStartRect.height + deltaY);
                        break;
                    case "n":
                        newRect.y = Math.min(resizeStartRect.y + deltaY, resizeStartRect.y + resizeStartRect.height - 10);
                        newRect.height = resizeStartRect.height - (newRect.y - resizeStartRect.y);
                        break;
                    case "s":
                        newRect.height = Math.max(10, resizeStartRect.height + deltaY);
                        break;
                    case "w":
                        newRect.x = Math.min(resizeStartRect.x + deltaX, resizeStartRect.x + resizeStartRect.width - 10);
                        newRect.width = resizeStartRect.width - (newRect.x - resizeStartRect.x);
                        break;
                    case "e":
                        newRect.width = Math.max(10, resizeStartRect.width + deltaX);
                        break;
                }
                
                // No clamping when resizing
                
                // Update rectangle
                persistentRect = newRect;
                persistentRectCanvas.requestPaint();
            }
        }

        onReleased: function (mouse) {
            if (isPersistentRectSelecting) {
                isPersistentRectSelecting = false;
                
                // Calculate final persistent rectangle in video pixel coordinates
                var rect = Qt.rect(
                    Math.min(persistentRectStart.x, persistentRectEnd.x),
                    Math.min(persistentRectStart.y, persistentRectEnd.y),
                    Math.abs(persistentRectEnd.x - persistentRectStart.x),
                    Math.abs(persistentRectEnd.y - persistentRectStart.y)
                );
                
                // No clamping on finalize
                
                // Only create rectangle if it has meaningful size
                if (rect.width > 5 && rect.height > 5) {
                    hasPersistentRect = true;
                    persistentRect = rect;
                    persistentRectCanvas.requestPaint();
                }
                
                // Clear creation state
                persistentRectStart = Qt.point(0, 0);
                persistentRectEnd = Qt.point(0, 0);
                // Ensure any dashed selection is cleared if rectangle wasn't created
                if (!hasPersistentRect) {
                    persistentRectCanvas.requestPaint();
                }
            } else if (isDraggingPersistentRect) {
                // End dragging the persistent rectangle
                isDraggingPersistentRect = false;
                dragStartOffset = Qt.point(0, 0);
            } else if (isResizingRect) {
                // End resizing the persistent rectangle
                isResizingRect = false;
                resizeHandle = "";
                resizeStartPoint = Qt.point(0, 0);
                resizeStartRect = Qt.rect(0, 0, 0, 0);
            }
        }

        onDoubleClicked: function (mouse) {
            if (hasPersistentRect) {
                // End any ongoing drag/resize/select to ensure double-click is handled
                isPersistentRectSelecting = false;
                isDraggingPersistentRect = false;
                isResizingRect = false;
                // Check if double-click is inside the persistent rectangle
                var screenPoint = Qt.point(mouse.x, mouse.y);
                var screenRect = convertVideoToScreenCoordinates(persistentRect);
                
                if (isPointInScreenRect(screenPoint, screenRect)) {
                    // Convert rectangle to screen coordinates for zoom
                    var rect = convertVideoToScreenCoordinates(persistentRect);
                    
                    // Zoom to the rectangle
                    if (videoWindow.sharedView) {
                        videoWindow.sharedView.zoomToSelection(rect, pixelValuesCanvas.getVideoRect(), 
                                                             videoWindow.sharedView.zoom, 
                                                             videoWindow.sharedView.centerX, 
                                                             videoWindow.sharedView.centerY);
                    }
                    
                    // Clear the persistent rectangle
                    removePersistentRect();
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
                if (videoWindow.hasPersistentRect) {
                    // Convert video pixel coordinates to screen coordinates for drawing
                    var screenRect = convertVideoToScreenCoordinates(persistentRect);
                    ctx.fillStyle = "rgba(255, 0, 0, 0.1)";
                    ctx.fillRect(screenRect.x, screenRect.y, screenRect.width, screenRect.height);
                    ctx.strokeStyle = "red";
                    ctx.lineWidth = 2;
                    ctx.strokeRect(screenRect.x, screenRect.y, screenRect.width, screenRect.height);
                    
                    // Draw resize handles
                    drawResizeHandles(ctx, screenRect);
                    
                    // Draw coordinate text
                    var coordText = persistentRect.width + ":" + persistentRect.height + ":" + persistentRect.x + ":" + persistentRect.y;
                    drawCoordinateText(ctx, screenRect, coordText);
                }
                
                // Draw persistent rectangle during creation
                if (videoWindow.isPersistentRectSelecting) {
                    // Convert video pixel coordinates to screen coordinates for drawing
                    var rect = Qt.rect(
                        Math.min(persistentRectStart.x, persistentRectEnd.x),
                        Math.min(persistentRectStart.y, persistentRectEnd.y),
                        Math.abs(persistentRectEnd.x - persistentRectStart.x),
                        Math.abs(persistentRectEnd.y - persistentRectStart.y)
                    );
                    var screenRect = convertVideoToScreenCoordinates(rect);
                    ctx.fillStyle = "rgba(255, 0, 0, 0.2)";
                    ctx.fillRect(screenRect.x, screenRect.y, screenRect.width, screenRect.height);
                    ctx.strokeStyle = "red";
                ctx.lineWidth = 2;
                    ctx.setLineDash([5, 5]);
                    ctx.strokeRect(screenRect.x, screenRect.y, screenRect.width, screenRect.height);
                    ctx.setLineDash([]);
                    
                    // Draw coordinate text during creation
                    var coordText = rect.width + ":" + rect.height + ":" + rect.x + ":" + rect.y;
                    drawCoordinateText(ctx, screenRect, coordText);
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
                ctx.font = Math.floor(fontSize) + "px Arial";

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
                ctx.font = "12px Arial";
            } catch (error) {
                console.log("QML: drawPixelValue error:", error);
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
        var meta = videoWindow.getFrameMeta();
        if (!meta || !meta.yWidth || !meta.yHeight)
            return Qt.rect(0, 0, 0, 0);
            
        var screenRect = pixelValuesCanvas.getVideoRect();
        
        // Convert video pixel coordinates to normalized coordinates (0-1)
        var normalizedX = videoRect.x / meta.yWidth;
        var normalizedY = videoRect.y / meta.yHeight;
        var normalizedWidth = videoRect.width / meta.yWidth;
        var normalizedHeight = videoRect.height / meta.yHeight;
        
        // Apply zoom and pan transformation
        if (videoWindow.sharedView) {
            normalizedX = (normalizedX - videoWindow.sharedView.centerX) * videoWindow.sharedView.zoom + 0.5;
            normalizedY = (normalizedY - videoWindow.sharedView.centerY) * videoWindow.sharedView.zoom + 0.5;
            normalizedWidth *= videoWindow.sharedView.zoom;
            normalizedHeight *= videoWindow.sharedView.zoom;
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
        var meta = videoWindow.getFrameMeta();
        if (!meta || !meta.yWidth || !meta.yHeight)
            return Qt.point(0, 0);
        
        var videoRect = pixelValuesCanvas.getVideoRect();
        
        // Convert screen coordinates to normalized video coordinates (0-1)
        var normalizedX = (screenPoint.x - videoRect.x) / videoRect.width;
        var normalizedY = (screenPoint.y - videoRect.y) / videoRect.height;
        
        // Apply inverse zoom and pan transformation to get actual video coordinates
        if (videoWindow.sharedView) {
            // Inverse transformation: account for current view state
            normalizedX = (normalizedX - 0.5) / videoWindow.sharedView.zoom + videoWindow.sharedView.centerX;
            normalizedY = (normalizedY - 0.5) / videoWindow.sharedView.zoom + videoWindow.sharedView.centerY;
        }
        
        // Convert to actual pixel coordinates
        var pixelX = Math.round(normalizedX * meta.yWidth);
        var pixelY = Math.round(normalizedY * meta.yHeight);
        
        return Qt.point(pixelX, pixelY);
    }

    // Get current visible video bounds (pixel coordinates) from sharedView
    function getVisibleVideoBounds() {
        var meta = videoWindow.getFrameMeta();
        if (!meta || !meta.yWidth || !meta.yHeight || !videoWindow.sharedView)
            return Qt.rect(0, 0, 0, 0);

        var zoom = Math.max(1.0, videoWindow.sharedView.zoom);
        var cx = videoWindow.sharedView.centerX;
        var cy = videoWindow.sharedView.centerY;

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
            ctx.font = "12px monospace";
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
            console.log("QML: drawCoordinateText error:", error);
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
            console.log("QML: drawResizeHandles error:", error);
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
            font.family: "Arial"
            font.pixelSize: 12
            text: {
                var durationSec = Math.floor(videoWindow.duration / 1000);
                var currentSec = Math.floor(videoWindow.currentTimeMs / 1000);
                var durationMin = Math.floor(durationSec / 60);
                var currentMin = Math.floor(currentSec / 60);
                var durationStr = durationMin + ":" + (durationSec % 60).toString().padStart(2, '0');
                var currentStr = currentMin + ":" + (currentSec % 60).toString().padStart(2, '0');

                var osdStr = "Time: " + currentStr + " / " + durationStr + "\n" + "Frame: " + videoWindow.currentFrame;
                if (videoWindow.osdState === 1) {
                    // Basic info: duration and current frame
                    return osdStr;
                } else if (videoWindow.osdState === 2) {
                    // Detailed info: add resolution, timebase, aspect ratio, codec, color space, and range
                    return osdStr + "\n" + "Resolution: " + videoWindow.videoResolution + "\n" + "Timebase: " + videoWindow.timeBase + "\n" + "Aspect: " + videoWindow.getAspectRatio.toFixed(2) + "\n" + "Codec: " + videoWindow.codecName + "\n" + "Color Space: " + videoWindow.colorSpace + "\n" + "Color Range: " + videoWindow.colorRange;
                }
                return "";
            }
        }
    }

    // Loading Indicator
    Rectangle {
        visible: videoController && (!videoController.ready || videoController.isSeeking || videoController.isBuffering)
        anchors.centerIn: parent
        width: loadingText.width + 40
        height: loadingText.height + 20
        color: "black"
        opacity: 0.8
        radius: 8
        z: 102

        Text {
            id: loadingText
            anchors.centerIn: parent
            color: "white"
            font.family: "Arial"
            font.pixelSize: 14
            text: videoController && videoController.isSeeking ? "Seeking..." : "Loading..."
            horizontalAlignment: Text.AlignHCenter
        }
    }
}
