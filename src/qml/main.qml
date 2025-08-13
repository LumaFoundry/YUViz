import QtQuick.Window 6.0
// Use standard Controls for native styling when QQuickStyle is set
import QtQuick.Controls 6.0
import QtQuick 6.0
import QtQuick.Layouts 1.15
import Theme 1.0
import NativeAbout 1.0

ApplicationWindow {
    id: mainWindow

    property int videoCount: 0
    property var videoWindows: []
    property int globalOsdState: 0
    property string videoOriginalName: ""
    // Detect if we should prefer native styling (macOS / Windows)
    readonly property bool nativeStyle: (Qt.platform.os === "osx" || Qt.platform.os === "windows")

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

    title: APP_NAME
    width: 800
    height: 600
    // Force dark app background regardless of native style
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

    property var diffEmbeddedInstance: null
    property var parkedSecondVideo: null
    property int leftVideoIdForDiff: -1
    property int rightVideoIdForDiff: -1
    property bool closingDiffPopupForEmbed: false

    Item {
        id: hiddenBin
        visible: false
        width: 0
        height: 0
    }

    Component {
        id: diffEmbeddedComponent
        DiffPane {}
    }

    ImportPopup {
        id: importDialog
        mainWindow: mainWindow
        anchors.centerIn: parent
        onVideoImported: function (filePath, width, height, fps, format) {
            importVideoFromParams(filePath, width, height, fps, format, false);
        }
    }

    // Clear all persistent rectangles across all windows to avoid potential segfaults
    function clearAllRectangles() {
        try {
            // Clear rectangles in all video windows
            for (let i = 0; i < videoWindowContainer.children.length; ++i) {
                const child = videoWindowContainer.children[i];
                if (child && child.removePersistentRect) {
                    child.removePersistentRect();
                }
            }
            // Clear rectangle in diff window if exists
            if (diffPopupInstance && diffPopupInstance.removePersistentRect) {
                diffPopupInstance.removePersistentRect();
            }
            // Clear rectangle in embedded diff pane if exists
            if (diffEmbeddedInstance && diffEmbeddedInstance.removePersistentRect) {
                diffEmbeddedInstance.removePersistentRect();
            }
        } catch (e) {
            console.log("[mainWindow] clearAllRectangles error:", e);
        }
    }

    CommandsPopup {
        id: commandsDialog
    }

    AboutPage {
        id: aboutDialog
        visible: Qt.platform.os !== "osx" && aboutDialog.visible
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
            errorDialog.title = title;
            errorDialogText.text = message;
            errorDialog.open();
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
            if (event.isAutoRepeat)
                return;

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

            // 'O' key shortcut for OSD now handled via Menu Action shortcut to avoid double triggering here.

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

    // Whenever Ctrl state toggles on, ensure only one rectangle globally
    onIsCtrlPressedChanged: {
        if (isCtrlPressed) {
            clearAllRectangles();
        }
    }

    menuBar: MenuBar {
        font.pixelSize: Theme.fontSizeNormal

        Menu {
            title: "File"
            Action {
                text: "Open New Video"
                onTriggered: {
                    // Close all rectangles before opening new video set
                    clearAllRectangles();
                    destroyTimer.destroyIndex = videoWindowContainer.children.length - 1;
                    destroyTimer.start();
                }
            }
            Action {
                text: "Add Video"
                enabled: videoCount < 2
                onTriggered: {
                    // Close all rectangles before adding a new video window
                    clearAllRectangles();
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
        // Newly added View menu with OSD toggle
        Menu {
            title: "View"
            Action {
                text: "OSD"
                shortcut: "O" // Displays 'O' on the right side (mac-style) and triggers the action
                onTriggered: mainWindow.toggleGlobalOsdState()
            }
        }
        Menu {
            title: "Help"
            Action {
                text: "Show all Commands"
                onTriggered: commandsDialog.visible = true
            }
            Action {
                text: "About"
                onTriggered: {
                    if (Qt.platform.os === "osx") {
                        NativeAbout.showNativeAbout(APP_NAME, APP_VERSION, BUILD_DATE);
                    } else {
                        aboutDialog.visible = true;
                    }
                }
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
            color: "#2a2a2a"

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
                color: "#2a2a2a"
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

                    RowLayout {
                        id: transportButtons
                        spacing: 0

                        Button {
                            id: stepBackwardButton
                            text: "←"
                            Layout.preferredWidth: Theme.iconSize
                            Layout.preferredHeight: Theme.iconSize
                            font.pixelSize: Theme.fontSizeNormal
                            font.bold: true
                            background: Rectangle { color: "#2f2f2f" }
                            contentItem: Text {
                                text: stepBackwardButton.text
                                font.pixelSize: stepBackwardButton.font.pixelSize
                                font.bold: true
                                color: Theme.iconColor
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                anchors.centerIn: parent
                            }
                            onClicked: {
                                if (videoController) videoController.stepBackward();
                                keyHandler.forceActiveFocus();
                            }
                            ToolTip.visible: hovered
                            ToolTip.text: "Step Backward (Left Arrow)"
                        }

                        Button {
                            id: playPauseButton
                            text: videoController ? (videoController.isPlaying ? "⏸" : "▶") : "▶"
                            Layout.preferredWidth: Theme.iconSize
                            Layout.preferredHeight: Theme.iconSize
                            background: Rectangle { color: "#3a3a3a" }
                            font.bold: true
                            contentItem: Text {
                                text: playPauseButton.text
                                font.pixelSize: playPauseButton.font.pixelSize
                                font.bold: true
                                color: Theme.iconColor
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                anchors.centerIn: parent
                            }
                            onClicked: {
                                videoController.togglePlayPause();
                                keyHandler.focus = true; //Do not change, Windows requires
                            }
                            ToolTip.visible: hovered
                            ToolTip.text: videoController && videoController.isPlaying ? "Pause (Space)" : "Play (Space)"
                        }

                        Button {
                            id: stepForwardButton
                            text: "→"
                            Layout.preferredWidth: Theme.iconSize
                            Layout.preferredHeight: Theme.iconSize
                            font.pixelSize: Theme.fontSizeNormal
                            font.bold: true
                            background: Rectangle { color: "#2f2f2f" }
                            contentItem: Text {
                                text: stepForwardButton.text
                                font.pixelSize: stepForwardButton.font.pixelSize
                                font.bold: true
                                color: Theme.iconColor
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                anchors.centerIn: parent
                            }
                            onClicked: {
                                if (videoController) videoController.stepForward();
                                keyHandler.forceActiveFocus();
                            }
                            ToolTip.visible: hovered
                            ToolTip.text: "Step Forward (Right Arrow)"
                        }
                    }

                    // Speed selector combobox
                    RowLayout {
                        spacing: 6
                        Text {
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
                    // Grouped layout with reduced spacing for Reset / Diff / Jump / Direction
                    RowLayout {
                        id: analysisButtons
                        spacing: 5

                        // Reset View Button
                        Button {
                            text: "Reset View"
                            Layout.preferredHeight: Theme.buttonHeight
                            font.pixelSize: Theme.fontSizeMedium
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
                        id: diffButton
                        text: "Diff"
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
                            videoController.pause();
                            // Always close rectangles before toggling diff
                            clearAllRectangles();

                            // Toggle diff window: if open, close it; if closed, open it
                            if (diffPopupInstance && diffPopupInstance.visible) {
                                // Ensure diff window rectangle is cleared before hide
                                if (diffPopupInstance.removePersistentRect) {
                                    diffPopupInstance.removePersistentRect();
                                }
                                diffPopupInstance.visible = false;
                                keyHandler.forceActiveFocus();

                                videoController.pause();
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
                                    // Capture the ids (store them on the instance when you create it)
                                    const leftId = diffPopupInstance.leftVideoId;
                                    const rightId = diffPopupInstance.rightVideoId;

                                    if (!mainWindow.closingDiffPopupForEmbed) {
                                        // Only disable diff mode if we're closing the popup for real
                                        videoController.setDiffMode(false, leftId, rightId);
                                    }

                                    // Clear rectangle before destroy
                                    if (diffPopupInstance.removePersistentRect) {
                                        diffPopupInstance.removePersistentRect();
                                    }

                                    diffPopupInstance.destroy();
                                    diffPopupInstance = null;
                                    mainWindow.closingDiffPopupForEmbed = false;
                                }
                            });
                            videoLoader.setupDiffWindow(leftId, rightId);
                            diffPopupInstance.diffVideoWindow.osdState = mainWindow.globalOsdState;
                            keyHandler.forceActiveFocus();
                        }
                    }

                        // Jump To Frame button (mirrors 'J' key behavior)
                        Button {
                            text: "Jump To"
                            Layout.preferredHeight: Theme.buttonHeight
                            font.pixelSize: Theme.fontSizeMedium
                            enabled: videoWindowContainer.children.length > 0 && videoController
                            onClicked: {
                                jumpPopup.open();
                                keyHandler.forceActiveFocus();
                            }
                        }

                        Button {
                            id: directionToggleButton
                            text: videoController && videoController.isForward ? "⏵⏵" : "⏴⏴"
                            Layout.preferredWidth: Theme.iconSize
                            Layout.preferredHeight: Theme.iconSize
                            font.pixelSize: playPauseButton.font.pixelSize
                            font.bold: true
                            background: Rectangle { color: "#3a3a3a" }
                            contentItem: Text {
                                text: directionToggleButton.text
                                font.pixelSize: directionToggleButton.font.pixelSize
                                font.bold: true
                                color: Theme.iconColor
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                anchors.centerIn: parent
                            }
                            Accessible.name: "Playback Direction"
                            Accessible.description: videoController && videoController.isForward ? "Forward" : "Reverse"
                            onClicked: {
                                if (videoController) {
                                    videoController.toggleDirection();
                                    keyHandler.forceActiveFocus();
                                }
                            }
                            ToolTip.visible: hovered
                            ToolTip.text: videoController && videoController.isForward ? "Forward (click to switch to reverse)" : "Reverse (click to switch to forward)"
                        }
                    }

                    // Fullscreen toggle (styled like seek buttons)
                    Button {
                        id: fullscreenButton
                        Layout.preferredWidth: Theme.iconSize
                        Layout.preferredHeight: Theme.iconSize
                        // Single diagonal arrow glyph used for both enter/exit fullscreen
                        text: "⤢"
                        font.pixelSize: Theme.fontSizeLarge + 2
                            font.bold: true
                        background: Rectangle { color: "#2f2f2f" }
                        contentItem: Text {
                            text: fullscreenButton.text
                            font.pixelSize: fullscreenButton.font.pixelSize
                                font.bold: true
                            color: Theme.iconColor
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            anchors.centerIn: parent
                        }
                        onClicked: {
                            toggleFullScreen();
                            keyHandler.focus = true; // Do not change, Windows requires
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: mainWindow.visibility === Window.FullScreen ? "Exit Fullscreen" : "Enter Fullscreen"
                    }
                }

                // Time display with slider
                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10
                    Layout.preferredHeight: Theme.sliderHeight + Theme.sliderHandleSize / 2

                    // Current time display
                    Text {
                        id: currentTimeText
                        Layout.preferredWidth: 60
                        text: {
                            if (videoController) {
                                var currentSeconds = Math.floor(videoController.currentTimeMs / 1000);
                                var currentMin = Math.floor(currentSeconds / 60);
                                var currentSec = currentSeconds % 60;
                                return currentMin + ":" + (currentSec < 10 ? "0" : "") + currentSec;
                            }
                            return "0:00";
                        }
                        color: Theme.textColor
                        font.pixelSize: Theme.fontSizeSmall
                        horizontalAlignment: Text.AlignRight
                    }

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

                    // Total time display
                    Text {
                        id: totalTimeText
                        Layout.preferredWidth: 60
                        text: {
                            if (videoController) {
                                var totalSeconds = Math.floor(videoController.duration / 1000);
                                var totalMin = Math.floor(totalSeconds / 60);
                                var totalSec = totalSeconds % 60;
                                return totalMin + ":" + (totalSec < 10 ? "0" : "") + totalSec;
                            }
                            return "0:00";
                        }
                        color: Theme.textColor
                        font.pixelSize: Theme.fontSizeSmall
                        horizontalAlignment: Text.AlignLeft
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
            font.family: Theme.fontFamily
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

        if (diffEmbeddedInstance && (id === rightVideoIdForDiff || id === leftVideoIdForDiff)) {
            disableEmbeddedDiffAndRestore();
        }

        // Clear rectangles early to avoid dangling references during removal
        clearAllRectangles();

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
        if (diffEmbeddedInstance && diffEmbeddedInstance.diffVideoWindow) {
            diffEmbeddedInstance.diffVideoWindow.osdState = mainWindow.globalOsdState;
            videoWindowContainer.children[1].osdState = 0;
        }
    }

    function enableEmbeddedDiff() {
        videoController.pause();
        const videos = videoWindowContainer.children;
        if (videos.length < 2)
            return;

        let left = videos[0];
        let right = videos[1];
        if (!left || !right)
            return;
        // Prevent duplicates
        if (diffEmbeddedInstance)
            return;

        // Create embedded diff as overlay ON TOP of the right video instead of replacing it
        diffEmbeddedInstance = diffEmbeddedComponent.createObject(right, {});
        if (!diffEmbeddedInstance)
            return;

        // Hide OSD & title for second video
        videoWindowContainer.children[1].osdState = 0;
        videoOriginalName = videoWindowContainer.children[1].videoName;
        videoWindowContainer.children[1].videoDisplayName = "Difference";

        // Disable diff button
        diffButton.enabled = false;

        // Overlay fill
        diffEmbeddedInstance.z = 100; // ensure on top of underlying video content
        // Bind width/height to parent (right video window)
        diffEmbeddedInstance.width = right.width; // implicit binding
        diffEmbeddedInstance.height = right.height;
        right.widthChanged.connect(function () {
            if (diffEmbeddedInstance)
                diffEmbeddedInstance.width = right.width;
        });
        right.heightChanged.connect(function () {
            if (diffEmbeddedInstance)
                diffEmbeddedInstance.height = right.height;
        });
        diffEmbeddedInstance.visible = true;

        if (diffEmbeddedInstance.diffVideoWindow)
            diffEmbeddedInstance.diffVideoWindow.osdState = mainWindow.globalOsdState;

        leftVideoIdForDiff = left.videoId;
        rightVideoIdForDiff = right.videoId;

        // Wire controllers to the embedded diff instance
        videoLoader.setupDiffWindow(leftVideoIdForDiff, rightVideoIdForDiff);
        keyHandler.forceActiveFocus();
    }

    function disableEmbeddedDiffAndRestore() {
        if (diffEmbeddedInstance) {
            // stop diff mode for this pair
            videoController.setDiffMode(false, leftVideoIdForDiff, rightVideoIdForDiff);
            diffEmbeddedInstance.destroy();
            diffEmbeddedInstance = null;
        }
        leftVideoIdForDiff = -1;
        rightVideoIdForDiff = -1;

        // Restore OSD & title for second video
        videoWindowContainer.children[1].osdState = globalOsdState;
        videoWindowContainer.children[1].videoDisplayName = videoOriginalName;
        diffButton.enabled = true;

        keyHandler.forceActiveFocus();
    }

    function isEmbeddedDiffActive() {
        return diffEmbeddedInstance !== null;
    }
}
