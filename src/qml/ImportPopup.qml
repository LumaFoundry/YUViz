import QtQuick 6.0
import QtQuick.Controls.Basic 6.0
import QtQuick.Controls 6.2
import QtQuick.Layouts 6.0
import QtQuick.Dialogs 6.2
import VideoFormatUtils 1.0

Popup {
    id: importPopup
    width: Math.min(360, parent.width - 40)
    height: Math.min(400, parent.height - 40)
    modal: true
    focus: true
    clip: true
    x: (parent ? (parent.width - width) / 2 : 0)
    y: (parent ? (parent.height - height) / 2 : 0)

    property string mode: "new"  // "new" or "add"
    property string selectedFile: ""
    property bool isYUV: !VideoFormatUtils.isCompressedFormat(VideoFormatUtils.detectFormatFromExtension(selectedFile))
    property var mainWindow
    signal videoImported(string filePath, int width, int height, double fps, string pixelFormat)
    signal accepted
    // Helper to prefill UI from current selectedFile, used by drag-and-drop
    function prefillFromSelectedFile() {
        const urlStr = importPopup.selectedFile;
        if (!urlStr)
            return;
        const file = urlStr.split('/').pop();

        // Match resolution + fps like 1920x1080_25 or 1920x1080-25.0
        const match = file.match(/(\d{3,5})x(\d{3,5})[_-](\d{2,3}(?:\.\d{1,2})?)/);
        if (match) {
            resolutionInput.editText = match[1] + "x" + match[2];
            fpsInput.text = match[3];
        } else {
            const resMatch = file.match(/(\d{3,5})x(\d{3,5})/);
            if (resMatch) {
                resolutionInput.editText = resMatch[0];
            } else {
                resolutionInput.editText = "";
            }
            fpsInput.text = "";
        }

        autoSelectFormat(file);
    }

    // Component to be used as a template for creating the dialog
    Component {
        id: fileDialogComponent
        FileDialog {
            id: dialog
            title: "Choose a video file"
            options: FileDialog.DontUseNativeDialog
        }
    }

    function openFileDialog() {
        const compressedExtensions = "*.mp4 *.mkv *.avi *.mov *.webm *.hevc *.av1 *.264 *.265";
        const rawExts = VideoFormatUtils.getRawVideoExtensions();
        const allSupportedExts = VideoFormatUtils.getAllSupportedExtensions();
        const rawWildcards = rawExts.map(ext => "*" + ext).join(" ");
        const allSupportedWildcards = allSupportedExts.map(ext => "*" + ext).join(" ");
        const dynamicFilters = [
            `All Video Files (${allSupportedWildcards} ${compressedExtensions})`,
            `Raw YUV Files (${rawWildcards})`,
            `Y4M Files (*.y4m)`,
            `Compressed Video Files (${compressedExtensions})`,
            "All Files (*)"
        ];

        var dialog = fileDialogComponent.createObject(importPopup, {
            "parentWindow": importPopup.mainWindow,
            "nameFilters": dynamicFilters
        });

        dialog.accepted.connect(function() {
            // Keep URL as-is; C++ normalizes via QUrl::fromUserInput().toLocalFile()
            const urlStr = dialog.selectedFile.toString();
            importPopup.selectedFile = urlStr;
            const file = urlStr.split('/').pop();

            // Strict match: resolution followed by underscore/dash and FPS
            const match = file.match(/(\d{3,5})x(\d{3,5})[_-](\d{2,3}(?:\.\d{1,2})?)/);
            if (match) {
                resolutionInput.editText = match[1] + "x" + match[2];
                fpsInput.text = match[3];
            } else {
                // Fallback: just try to extract resolution anywhere
                const resMatch = file.match(/(\d{3,5})x(\d{3,5})/);
                if (resMatch) {
                    resolutionInput.editText = resMatch[0];
                } else {
                    resolutionInput.editText = "";
                }
                fpsInput.text = "";
            }

            // Auto-select format based on filename
            autoSelectFormat(file);
        });

        dialog.open();
    }

    background: Rectangle {
        color: "white"
        radius: 6
        border.color: "#aaa"
    }

    ScrollView {
        id: scrollView
        anchors.fill: parent
        anchors.margins: 8
        clip: true
    ColumnLayout {
        width: parent.width
        spacing: 12
        x: width < scrollView.width ? (scrollView.width - width) / 2 : 0

        Label {
            text: "Select a video file:"
            Layout.fillWidth: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            TextField {
                id: filePathInput
                text: importPopup.selectedFile
                placeholderText: "Path to file"
                readOnly: true
                Layout.fillWidth: true
            }

            Button {
                text: "Browse"
                onClicked: openFileDialog()
            }
        }

        Label {
            visible: isYUV
            text: "Resolution (e.g. 1920x1080):"
            Layout.fillWidth: true
        }

        ComboBox {
            id: resolutionInput
            visible: isYUV
            editable: true
            Layout.fillWidth: true
            model: ["3840x2160", "2560x1440", "1920x1080", "1280x720", "720x480", "640x360", "352x288", "176x144"]
            onActivated: {
                editText = currentText;
            }
        }

        Label {
            visible: isYUV
            text: "Frame rate (e.g. 25.0):"
            Layout.fillWidth: true
        }

        TextField {
            id: fpsInput
            visible: isYUV
            placeholderText: "25.0"
            Layout.fillWidth: true
            onAccepted: {
                if (fpsInput.text === "") {
                    fpsInput.text = fpsInput.placeholderText;
                }
            }
        }

        Label {
            visible: isYUV
            text: "Pixel Format"
            Layout.fillWidth: true
        }

        ComboBox {
            id: formatInput
            visible: isYUV
            model: {
                // Filter display names to only show YUV formats
                var allFormats = VideoFormatUtils.getDisplayNames();
                var allIds = VideoFormatUtils.getFormatIdentifiers();
                var yuvFormats = [];
                for (var i = 0; i < allIds.length; i++) {
                    if (!VideoFormatUtils.isCompressedFormat(allIds[i])) {
                        yuvFormats.push(allFormats[i]);
                    }
                }
                return yuvFormats;
            }
            Layout.fillWidth: true
            currentIndex: 0
            
            displayText: model[currentIndex]
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 8

            Button {
                text: "Cancel"
                onClicked: importPopup.close()
            }

            Button {
                text: mode === "add" ? "Add" : "Load"
                enabled: importPopup.selectedFile !== "" && (!isYUV || (filePathInput.text !== "" && resolutionInput.editText.match(/^\d+x\d+$/) && !isNaN(parseFloat(fpsInput.text))))
                onClicked: {
                    const res = resolutionInput.editText.split("x");
                    const filePath = importPopup.selectedFile;
                    let width = isYUV ? parseInt(res[0]) : 1920;
                    let height = isYUV ? parseInt(res[1]) : 1080;
                    let fps = isYUV ? parseFloat(fpsInput.text) : 25.0;
                    let format = isYUV ? getYuvIdentifierByIndex(formatInput.currentIndex) : "COMPRESSED";
                    importPopup.videoImported(filePath, width, height, fps, format);
                    importPopup.close();
                }
            }
        }
    }
    }
    
    function getYuvIdentifierByIndex(index) {
        // Get YUV format identifier by index
        var allIds = VideoFormatUtils.getFormatIdentifiers();
        var yuvIds = [];
        for (var i = 0; i < allIds.length; i++) {
            if (!VideoFormatUtils.isCompressedFormat(allIds[i])) {
                yuvIds.push(allIds[i]);
            }
        }
        return index >= 0 && index < yuvIds.length ? yuvIds[index] : "";
    }
    
    function findYuvFormatIndex(identifier) {
        // Find index of YUV format identifier
        var allIds = VideoFormatUtils.getFormatIdentifiers();
        var yuvIndex = 0;
        for (var i = 0; i < allIds.length; i++) {
            if (!VideoFormatUtils.isCompressedFormat(allIds[i])) {
                if (allIds[i] === identifier) {
                    return yuvIndex;
                }
                yuvIndex++;
            }
        }
        return -1;
    }

    function autoSelectFormat(filename) {
        if (!isYUV) return;
        
        // Use VideoFormatUtils to detect format from filename
        const detectedFormat = VideoFormatUtils.detectFormatFromExtension(filename);
        const formatIndex = findYuvFormatIndex(detectedFormat);
        
        if (formatIndex >= 0) {
            formatInput.currentIndex = formatIndex;
        } else {
            // Default to the first YUV format in the list (index 0).
            formatInput.currentIndex = 0;
        }        
    }
}
