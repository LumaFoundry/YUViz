import QtQuick.Window 6.0
import QtQuick.Controls.Basic 6.0
import QtQuick 6.0
import QtQuick.Layouts 1.15
import Window 1.0
import Theme 1.0

ApplicationWindow {
    id: mainWindow
    title: "videoplayer"
    width: 800
    height: 600
    color: Theme.backgroundColor
    visible: true
    flags: Qt.Window
    visibility: Window.Windowed

    property bool videoLoaded: false
    property bool isCtrlPressed: false
    property bool isSelecting: false
    property bool wasPlayingBeforeResize: false
    property point selectionStart: Qt.point(0, 0)
    property point selectionEnd: Qt.point(0, 0)
    property bool isProcessingSelection: false
    property bool isMouseDown: false
    property bool resizing: false

    ImportPopup {
        id: importDialog
        mainWindow: mainWindow
        onAccepted: {
            videoLoaded = true;
            keyHandler.forceActiveFocus();
        }
    }

    Timer {
        id: resizeDebounce
        interval: 100
        repeat: false
        onTriggered: {
            resizing = false;
            if (wasPlayingBeforeResize) {
                videoController.play();
            }
        }
    }

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

    // Top-level key listener
    Item {
        id: keyHandler
        anchors.fill: parent
        focus: true
        enabled: videoLoaded
        Keys.onPressed: event => {
            if (event.key === Qt.Key_Space) {
                // console.log("Space key pressed");
                videoController.togglePlayPause();
                event.accepted = true;
            }

            if (event.key == Qt.Key_Right) {
                // console.log("Right arrow key pressed");
                videoController.stepForward();
                event.accepted = true;
            }

            if (event.key == Qt.Key_Left) {
                // console.log("Left arrow key pressed");
                videoController.stepBackward();
                event.accepted = true;
            }

            if (event.key === Qt.Key_Control) {
                isCtrlPressed = true;
                event.accepted = true;
            }
        }

        Keys.onReleased: event => {
            if (event.key === Qt.Key_Control) {
                isCtrlPressed = false;
                event.accepted = true;
            }

            if (event.key === Qt.Key_Escape && mainWindow.visibility === Window.FullScreen) {
                mainWindow.visibility = Window.Windowed;
                event.accepted = true;
            }
        }
    }

    menuBar: MenuBar {
        background: Rectangle {
            color: "#96ffffff"
        }
        font.pixelSize: Theme.fontSizeNormal

        Menu {
            title: "File"
            Action {
                text: "Open"
                onTriggered: importDialog.open()
            }
            Action {
                text: "Exit"
                onTriggered: Qt.quit()
            }
        }
        Menu {
            title: "Help"
            Action {
                text: "About"
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        VideoWindow {
            id: videoWindow
            objectName: "videoWindow_0"
            Layout.fillWidth: true
            Layout.fillHeight: true

            maxZoom: 10000.0

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
        }

        ToolBar {
            background: Rectangle {
                color: "#5d383838"
            }
            enabled: videoLoaded
            Layout.fillWidth: true
            ColumnLayout {
                id: panel
                anchors.fill: parent
                anchors.margins: 10
                spacing: 0

                RowLayout {
                    id: controls
                    Layout.alignment: Qt.AlignHCenter
                    Layout.fillWidth: true
                    spacing: 10

                    Button {
                        id: playPauseButton
                        text: videoController.isPlaying ? "⏸" : "▶"
                        Layout.preferredWidth: Theme.iconSize
                        Layout.preferredHeight: Theme.iconSize
                        background: Rectangle {
                            color: "#5d383838"
                        }

                        contentItem: Text {
                            text: playPauseButton.text
                            font.pixelSize: playPauseButton.font.pixelSize
                            color: Theme.iconColor
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            anchors.centerIn: parent
                        }

                        onClicked: {
                            videoController.togglePlayPause();
                            keyHandler.focus = true; //Do not change, Windows requires
                        }
                    }

                    // Direction switch
                    RowLayout {
                        spacing: 6

                        Text {
                            text: "Direction:"
                            color: "white"
                            font.pixelSize: Theme.fontSizeSmall
                        }

                        Switch {
                            id: directionSwitch
                            checked: videoController.isForward
                            scale: 0.8
                            onToggled: {
                                videoController.toggleDirection();
                                keyHandler.forceActiveFocus();
                            }
                        }

                        Text {
                            text: directionSwitch.checked ? "▶ Forward" : "◀ Reverse"
                            color: "white"
                            verticalAlignment: Text.AlignVCenter
                            Layout.preferredWidth: 80
                            font.pixelSize: Theme.fontSizeSmall
                        }
                    }

                    // Speed selector combobox
                    RowLayout {
                        spacing: 6
                        Text {
                            text: "Speed:"
                            color: "white"
                            font.pixelSize: Theme.fontSizeSmall
                        }
                        ComboBox {
                            id: speedSelector
                            model: ["0.25x", "0.5x", "1.0x", "1.5x", "2.0x"]
                            currentIndex: 2
                            Layout.preferredWidth: 80
                            Layout.preferredHeight: Theme.comboBoxHeight
                            font.pixelSize: Theme.fontSizeSmall
                            displayText: model[currentIndex]

                            onCurrentIndexChanged: {
                                let speed = parseFloat(model[currentIndex].replace("x", ""));
                                videoController.setSpeed(speed);
                                keyHandler.forceActiveFocus();
                            }

                            onActivated: {
                                keyHandler.forceActiveFocus();
                            }
                        }
                    }

                    // Color Space selector combobox
                    RowLayout {
                        spacing: 6
                        Text {
                            text: "Color Space:"
                            color: "white"
                            font.pixelSize: Theme.fontSizeSmall
                        }
                        ComboBox {
                            id: colorSpaceSelector
                            model: ["BT709", "BT709 Full", "BT470BG", "BT470BG Full", "BT2020", "BT2020 Full"]
                            currentIndex: 0
                            Layout.preferredWidth: 140
                            Layout.preferredHeight: Theme.comboBoxHeight
                            font.pixelSize: Theme.fontSizeSmall
                            displayText: model[currentIndex]

                            onCurrentIndexChanged: {
                                // define color space and range mapping
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

                            onActivated: {
                                keyHandler.forceActiveFocus();
                            }
                        }
                    }

                    // Reset View Button
                    Button {
                        text: "Reset View"
                        Layout.preferredWidth: Theme.buttonWidth
                        Layout.preferredHeight: Theme.buttonHeight
                        font.pixelSize: Theme.fontSizeSmall
                        onClicked: {
                            videoWindow.resetZoom();
                            selectionStart = Qt.point(0, 0);
                            selectionEnd = Qt.point(0, 0);
                            isSelecting = false;
                            isProcessingSelection = false;
                            selectionCanvas.requestPaint();
                            keyHandler.focus = true;
                        }
                    }

                    // Fullscreen toggle
                    Button {
                        id: fullscreenButton
                        Layout.preferredWidth: Theme.iconSize
                        Layout.preferredHeight: Theme.iconSize
                        text: mainWindow.visibility === Window.FullScreen ? "🗗" : "⤢"
                        font.pixelSize: Theme.fontSizeNormal
                        background: Rectangle {
                            color: "#5d383838"
                        }

                        contentItem: Text {
                            text: fullscreenButton.text
                            font.pixelSize: fullscreenButton.font.pixelSize
                            color: Theme.iconColor
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            anchors.centerIn: parent
                        }

                        // Use the toggleFullScreen function to handle fullscreen state
                        onClicked: {
                            toggleFullScreen();
                            keyHandler.focus = true; // Do not change, Windows requires
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10
                    Layout.preferredHeight: Theme.sliderHeight + Theme.sliderHandleSize / 2

                    // Time Slider
                    Slider {
                        id: timeSlider
                        Layout.fillWidth: true
                        Layout.preferredHeight: Theme.sliderHeight

                        from: 0.0
                        to: videoController.duration
                        value: dragging ? value : videoController.currentTimeMs
                        topPadding: Theme.sliderHandleSize / 2
                        bottomPadding: Theme.sliderHandleSize / 2

                        property bool dragging: false

                        onPressedChanged: {
                            if (pressed) {
                                dragging = true;
                            } else {
                                // On release: first seek to current value, then reset dragging state
                                var finalPosition = value;
                                videoController.seekTo(finalPosition);
                                // console.log("Slider released, seeking to: " + finalPosition);

                                // Now change dragging state and return focus
                                dragging = false;
                                keyHandler.forceActiveFocus();
                            }
                        }

                        background: Rectangle {
                            x: timeSlider.leftPadding
                            y: timeSlider.topPadding + timeSlider.availableHeight / 2 - height / 2
                            width: timeSlider.availableWidth
                            height: 4
                            radius: 2
                            color: Theme.borderColor

                            Rectangle {
                                width: timeSlider.visualPosition * parent.width
                                height: parent.height
                                radius: 2
                                color: Theme.primaryColor
                            }
                        }

                        handle: Rectangle {
                            implicitWidth: Theme.sliderHandleSize
                            implicitHeight: Theme.sliderHandleSize
                            radius: Theme.sliderHandleSize / 2
                            x: timeSlider.leftPadding + timeSlider.visualPosition * (timeSlider.availableWidth - width)
                            y: timeSlider.topPadding + timeSlider.availableHeight / 2 - height / 2
                            color: Theme.primaryColor
                            border.color: Theme.borderColor
                            border.width: 1
                        }
                    }
                }
            }
        }
    }

    function toggleFullScreen() {
        if (mainWindow.visibility === Window.FullScreen) {
            mainWindow.visibility = Window.Windowed;
        } else {
            mainWindow.visibility = Window.FullScreen;
        }
    }

    Text {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 10
        color: Theme.textColor
        text: isCtrlPressed ? "Hold Ctrl key and drag mouse to draw rectangle selection area" : "Press Ctrl key to start selection area"
        font.pixelSize: Theme.fontSizeNormal
    }
}
