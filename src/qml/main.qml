import QtQuick.Window 6.0
import QtQuick.Controls.Basic 6.0
import QtQuick 6.0
import QtQuick.Layouts 1.15
import Theme 1.0

ApplicationWindow {
    id: mainWindow

    property int videoCount: 0
    property var videoWindows: []

    QtObject {
        id: qmlBridge
        objectName: "qmlBridge"

        function createVideoWindow(index) {
            let obj = videoWindowComponent.createObject(videoWindowContainer, {
                videoId: index,
                assigned: false
            });
            if (obj !== null) {
                obj.objectName = "videoWindow_" + index;
                mainWindow.videoCount += 1;
                obj.requestRemove.connect(mainWindow.removeVideoWindowById);
            }
            return obj;
        }
    }
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

    property string importedFilePath: ""
    property int importedWidth: 0
    property int importedHeight: 0
    property double importedFps: 0
    property string importedFormat: ""
    property bool importedAdd: false

    ImportPopup {
        id: importDialog
        onVideoImported: function (filePath, width, height, fps, format, add) {
            importVideoFromParams(filePath, width, height, fps, format, add);
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
        enabled: videoWindowContainer.children.length > 0
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
        font.pixelSize: Theme.fontSizeNormal

        Menu {
            title: "File"
            Action {
                text: "Open New Video"
                onTriggered: {
                    importDialog.mode = "new";
                    importDialog.open();
                    importDialog.openFileDialog();
                }
            }
            Action {
                text: "Add Video"
                onTriggered: {
                    importDialog.mode = "add";
                    importDialog.open();
                    importDialog.openFileDialog();
                }
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

        delegate: MenuBarItem {
            id: menuBarItem

            contentItem: Text {
                text: menuBarItem.text
                font: menuBarItem.font
                opacity: enabled ? 1.0 : 0.3
                color: menuBarItem.highlighted ? "#000000" : Theme.textColor
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }

            background: Rectangle {
                implicitWidth: 40
                implicitHeight: 40
                opacity: enabled ? 1 : 0.3
                color: menuBarItem.highlighted ? Theme.textColor : "transparent"
            }
        }

        background: Rectangle {
            implicitWidth: 40
            implicitHeight: 40
            color: "#5d383838"

            Rectangle {
                color: Theme.textColor
                width: parent.width
                height: 1
                anchors.bottom: parent.bottom
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Item {
            id: videoArea
            Layout.fillWidth: true
            Layout.fillHeight: true

            Grid {
                id: videoWindowContainer
                anchors.fill: parent
                spacing: 0
                columns: Math.ceil(Math.sqrt(mainWindow.videoCount))
            }

            Component {
                id: videoWindowComponent
                VideoWindow {
                    id: videoWindowInstance
                    width: (videoArea.width / videoWindowContainer.columns)
                    height: (videoArea.height / Math.ceil(mainWindow.videoCount / videoWindowContainer.columns))
                }
            }

            Rectangle {
                id: placeholder
                anchors.fill: parent
                color: "#181818"
                visible: videoWindowContainer.children.length === 0
                z: -1
                Text {
                    anchors.centerIn: parent
                    text: "No video loaded"
                    color: Theme.textColor
                    font.pointSize: Theme.fontSizeNormal
                }
            }
        }
        ToolBar {
            background: Rectangle {
                color: "#5d383838"
            }
            enabled: videoWindowContainer.children.length > 0
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
                        text: videoController ? (videoController.isPlaying ? "⏸" : "▶") : "▶"
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
                            checked: videoController ? videoController.isForward : true
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
                    // Reset View Button
                    Button {
                        text: "Reset View"
                        Layout.preferredWidth: Theme.buttonWidth
                        Layout.preferredHeight: Theme.buttonHeight
                        font.pixelSize: Theme.fontSizeSmall
                        onClicked: {
                            videoWindowLoader.item.resetZoom();
                            selectionStart = Qt.point(0, 0);
                            selectionEnd = Qt.point(0, 0);
                            isSelecting = false;
                            isProcessingSelection = false;
                            videoWindowLoader.item.resetSelectionCanvas();
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
                        to: videoController ? videoController.duration : 0
                        value: dragging ? value : videoController ? videoController.currentTimeMs : 0
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

    function importVideoFromParams(filePath, width, height, fps, format, add) {
        importedFilePath = filePath;
        importedWidth = width;
        importedHeight = height;
        importedFps = fps;
        importedFormat = format;
        importedAdd = add;
        videoLoader.loadVideo(importedFilePath, importedWidth, importedHeight, importedFps, importedFormat, true);
        videoLoaded = true;
        keyHandler.forceActiveFocus();
    }

    Text {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 10
        color: Theme.textColor
        text: isCtrlPressed ? "Hold Ctrl key and drag mouse to draw rectangle selection area" : "Press Ctrl key to start selection area"
        font.pixelSize: Theme.fontSizeNormal
    }

    function removeVideoWindowById(id) {
        console.log("[removeVideoWindowById] Called with id:", id);
        if (videoWindowContainer.children[id]) {
            videoWindowContainer.children[id].destroy();
            videoCount--;
        }
        videoController.removeVideo(id);
    }
}
