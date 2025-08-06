import QtQuick.Window 6.0
import QtQuick.Controls.Basic 6.0
import QtQuick 6.0
import QtQuick.Layouts 1.15
import Theme 1.0

ApplicationWindow {
    id: mainWindow

    property int videoCount: 0
    property var videoWindows: []
    property int globalOsdState: 0

    QtObject {
        id: qmlBridge
        objectName: "qmlBridge"

        function createVideoWindow(index) {
            console.log("[qmlBridge] createVideoWindow called with index:", index);
            let obj = videoWindowComponent.createObject(videoWindowContainer, {
                videoId: index,
                assigned: false
            });
            if (obj !== null) {
                obj.objectName = "videoWindow_" + index;
                mainWindow.videoCount += 1;
                obj.requestRemove.connect(mainWindow.removeVideoWindowById);

                // Sync OSD state with existing video windows
                obj.osdState = mainWindow.globalOsdState;
                console.log("[qmlBridge] Synced OSD state to:", mainWindow.globalOsdState);
            }
            return obj;
        }

        function createDiffWindow() {
            let obj = videoWindowComponent.createObject(diffPopupComponent, {
                videoId: -1,
                assigned: true
            });
            obj.objectName = "diffWindow";
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
    property bool wasPlayingBeforeResize: false
    property bool resizing: false

    property string importedFilePath: ""
    property int importedWidth: 0
    property int importedHeight: 0
    property double importedFps: 0
    property string importedFormat: ""

    property var diffPopupInstance: null

    ImportPopup {
        id: importDialog
        mainWindow: mainWindow
        anchors.centerIn: parent
        onVideoImported: function (filePath, width, height, fps, format) {
            importVideoFromParams(filePath, width, height, fps, format, false);
        }
    }

    Dialog {
        id: errorDialog
        title: "Error"
        modal: true
        standardButtons: Dialog.Ok
        width: 400
        anchors.centerIn: parent

        Label {
            id: errorDialogText
            text: "An unknown error occurred."
            anchors.fill: parent
            wrapMode: Label.WordWrap
            padding: 10
        }
    }

    Connections {
        target: videoLoader

        function onVideoLoadFailed(title, message) {
            errorDialog.title = title
            errorDialogText.text = message
            errorDialog.open()
        }
    }

    Component {
        id: diffPopupComponent
        DiffWindow {}
    }

    MismatchWarningPopup {
        id: mismatchWarningDialog
        newWidth: importedWidth
        newHeight: importedHeight
    }

    JumpToFramePopup {
        id: jumpPopup
        anchors.centerIn: parent
        maxFrames: videoController ? videoController.totalFrames : 0
        onJumpToFrame: function (frameNumber) {
            videoController.jumpToFrame(frameNumber);
        }
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

    Timer {
        id: destroyTimer
        interval: 100
        repeat: true
        running: false
        property int destroyIndex: -1
        onTriggered: {
            if (destroyIndex >= 0) {
                let child = videoWindowContainer.children[destroyIndex];
                if (child && child.videoId !== undefined) {
                    mainWindow.removeVideoWindowById(child.videoId);
                }
                destroyIndex--;
            } else {
                destroyTimer.stop();
                importDialog.mode = "new";
                importDialog.open();
                importDialog.openFileDialog();
            }
            sharedViewProperties.reset();
            selectionStart = Qt.point(0, 0);
            selectionEnd = Qt.point(0, 0);
            keyHandler.focus = true;
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

            if (event.key === Qt.Key_O) {
                console.log("O key pressed, toggling global OSD state");
                mainWindow.toggleGlobalOsdState();
                event.accepted = true;
            }

            if (event.key === Qt.Key_J) {
                jumpPopup.open();
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
                    destroyTimer.destroyIndex = videoWindowContainer.children.length - 1;
                    destroyTimer.start();
                }
            }
            Action {
                text: "Add Video"
                enabled: videoCount < 2
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

                    onMetadataInitialized: {
                        metadataReady = true;
                        // Check if this window is the last one added
                        if (this === videoWindowContainer.children[videoWindowContainer.children.length - 1]) {
                            if (mainWindow.videoCount > 1) {
                                const firstVideoWindow = videoWindowContainer.children[0];
                                const firstMeta = firstVideoWindow.getFrameMeta();
                                const thisMeta = this.getFrameMeta();

                                if (firstMeta && thisMeta) {
                                    const resMismatch = firstMeta.yWidth !== thisMeta.yWidth || firstMeta.yHeight !== thisMeta.yHeight;
                                    let fpsMismatch = false;
                                    let firstFps = 0;
                                    let thisFps = 0;

                                    {
                                        const firstTimeBaseString = firstVideoWindow.timeBase;
                                        const thisTimeBaseString = this.timeBase;
                                        const thisParts = thisTimeBaseString.split('/');
                                        const firstParts = firstTimeBaseString.split('/');
                                        const firstNum = parseInt(firstParts[0], 10);
                                        const firstDen = parseInt(firstParts[1], 10);
                                        const thisNum = parseInt(thisParts[0], 10);
                                        const thisDen = parseInt(thisParts[1], 10);

                                        if (firstNum > 0) {
                                            firstFps = (firstDen / firstNum).toFixed(2);
                                        }
                                        if (thisNum > 0) {
                                            thisFps = (thisDen / thisNum).toFixed(2);
                                        }
                                    }

                                    if (firstFps !== thisFps) {
                                        fpsMismatch = true;
                                    }

                                    // Trigger popup if either resolution or FPS don't match
                                    if (resMismatch || fpsMismatch) {
                                        mismatchWarningDialog.firstWidth = firstMeta.yWidth;
                                        mismatchWarningDialog.firstHeight = firstMeta.yHeight;
                                        mismatchWarningDialog.newWidth = thisMeta.yWidth;
                                        mismatchWarningDialog.newHeight = thisMeta.yHeight;
                                        mismatchWarningDialog.firstFps = firstFps;
                                        mismatchWarningDialog.newFps = thisFps;
                                        mismatchWarningDialog.open();
                                    }
                                }
                            }
                        }
                    }
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
                            model: ["2.0x", "1.5x", "1.0x", "0.5x", "0.25x"]
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
                            sharedViewProperties.reset();

                            for (var i = 0; i < videoWindowContainer.children.length; ++i) {
                                var child = videoWindowContainer.children[i];
                                if (child && child.resetSelectionCanvas) {
                                    child.resetSelectionCanvas();
                                }
                            }

                            keyHandler.focus = true;
                        }
                    }

                    Button {
                        text: "Diff"
                        Layout.preferredWidth: Theme.buttonWidth
                        Layout.preferredHeight: Theme.buttonHeight
                        font.pixelSize: Theme.fontSizeSmall
                        enabled: {
                            const videos = videoWindowContainer.children;

                            if (videos.length < 2) {
                                return false;
                            }

                            const firstVideo = videos[0];
                            if (!firstVideo || !firstVideo.metadataReady)
                                return false;
                            const firstMeta = firstVideo.getFrameMeta();
                            if (!firstMeta)
                                return false;

                            // Check all subsequent videos against the first one.
                            for (let i = 1; i < videos.length; i++) {
                                const currentVideo = videos[i];

                                if (!currentVideo || !currentVideo.metadataReady)
                                    return false;
                                const currentMeta = currentVideo.getFrameMeta();
                                if (!currentMeta)
                                    return false;

                                // If mismatch, disable the button.
                                if (firstMeta.yWidth !== currentMeta.yWidth || firstMeta.yHeight !== currentMeta.yHeight) {
                                    return false;
                                }
                            }

                            // All videos have the same resolution.
                            return true;
                        }

                        onClicked: {
                            // Only one diff at a time
                            if (diffPopupInstance && diffPopupInstance.visible) {
                                diffPopupInstance.raise();
                                return;
                            }

                            let leftId = videoWindowContainer.children[0].videoId;
                            let rightId = videoWindowContainer.children[1].videoId;

                            console.log("Creating diffPopupInstance with leftId:", leftId, "and rightId:", rightId);

                            // Pass video IDs
                            diffPopupInstance = diffPopupComponent.createObject(mainWindow, {
                                leftVideoId: leftId,
                                rightVideoId: rightId
                            });
                            diffPopupInstance.objectName = "diffPopupInstance";
                            console.log("Created diffPopupInstance:", diffPopupInstance, "objectName:", diffPopupInstance.objectName);

                            diffPopupInstance.visible = true;
                            // Cleanup when window closes
                            diffPopupInstance.onVisibleChanged.connect(function () {
                                if (!diffPopupInstance.visible) {
                                    diffPopupInstance.destroy();
                                    videoController.setDiffMode(false, leftId, rightId);
                                    diffPopupInstance = null;
                                }
                            });
                            videoLoader.setupDiffWindow(leftId, rightId);
                            diffPopupInstance.diffVideoWindow.osdState = mainWindow.globalOsdState;
                            keyHandler.forceActiveFocus();
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

    function importVideoFromParams(filePath, width, height, fps, format, forceSoftware) {
        importedFilePath = filePath;
        importedWidth = width;
        importedHeight = height;
        importedFps = fps;
        importedFormat = format;

        console.log("[importVideoFromParams] calling videoLoader");
        videoLoader.loadVideo(importedFilePath, importedWidth, importedHeight, importedFps, importedFormat, forceSoftware);
        videoLoaded = true;
        keyHandler.forceActiveFocus();
    }

    // Global Zoom Level Indicator
    Rectangle {
        visible: sharedViewProperties && sharedViewProperties.isZoomed && sharedViewProperties.zoom > 1.0
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.bottomMargin: 110 // 10 pixels above the Ctrl text (which is at bottomMargin: 80, plus text height ~20)
        width: globalZoomText.width + 16
        height: globalZoomText.height + 12
        color: "black"
        opacity: 0.8
        radius: 4
        z: 99

        Text {
            id: globalZoomText
            anchors.centerIn: parent
            color: "white"
            font.family: "monospace"
            font.pixelSize: 11
            text: sharedViewProperties ? "Zoom: " + (sharedViewProperties.zoom * 100).toFixed(0) + "%" : ""
        }
    }

    Text {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.bottomMargin: 80
        color: Theme.textColor
        text: isCtrlPressed ? "Hold Ctrl key and drag mouse to draw rectangle selection area" : "Press Ctrl key to start selection area"
        font.pixelSize: Theme.fontSizeNormal
    }

    function removeVideoWindowById(id) {
        console.log("[removeVideoWindowById] Called with id:", id);
        if (diffPopupInstance && (id === diffPopupInstance.leftVideoId || id === diffPopupInstance.rightVideoId)) {
            console.log("[removeVideoWindowById] Removing diff mode for video IDs:", diffPopupInstance.leftVideoId, diffPopupInstance.rightVideoId);
            videoController.setDiffMode(false, diffPopupInstance.leftVideoId, diffPopupInstance.rightVideoId);
            diffPopupInstance.visible = false;
        }
        for (let i = 0; i < videoWindowContainer.children.length; ++i) {
            let child = videoWindowContainer.children[i];
            if (child.videoId === id) {
                child.destroy();
                mainWindow.videoCount--;
                break;
            }
        }

        // If no windows left, reset global OSD state
        if (videoWindowContainer.children.length === 0) {
            mainWindow.globalOsdState = 0;
            console.log("[mainWindow] Reset global OSD state to 0");
        }

        videoController.removeVideo(id);
        keyHandler.forceActiveFocus();
    }

    function toggleGlobalOsdState() {
        mainWindow.globalOsdState = (mainWindow.globalOsdState + 1) % 3;
        for (let i = 0; i < videoWindowContainer.children.length; ++i) {
            let child = videoWindowContainer.children[i];
            if (child && child.osdState !== undefined) {
                child.osdState = mainWindow.globalOsdState;
            }
        }
        if (diffPopupInstance && diffPopupInstance.diffVideoWindow) {
            diffPopupInstance.diffVideoWindow.osdState = mainWindow.globalOsdState;
        }
    }
}
