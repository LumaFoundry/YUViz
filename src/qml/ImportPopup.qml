import QtQuick 6.0
import QtQuick.Controls.Basic 6.0
import QtQuick.Controls 6.2
import QtQuick.Layouts 6.0
import QtQuick.Dialogs 6.2

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
    property bool isYUV: selectedFile.toLowerCase().endsWith(".yuv")
    property var mainWindow
    signal videoImported(string filePath, int width, int height, double fps, string pixelFormat)
    signal accepted

    function openFileDialog() {
        fileDialog.open();
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

            FileDialog {
                id: fileDialog
                title: "Choose a video file"
                parentWindow: mainWindow
                options: FileDialog.DontUseNativeDialog
                nameFilters: ["All Video Files (*.yuv *.y4m *.mp4 *.mkv *.avi *.mov *.webm *.hevc *.av1 *.264 *.265)", "Raw YUV Files (*.yuv *.y4m)", "Compressed Video Files (*.mp4 *.mkv *.avi *.mov *.webm *.hevc *.av1 *.264 *.265)", "All Files (*)"]

                onAccepted: {
                    importPopup.selectedFile = selectedFile.toString().replace("file://", "");
                    const file = importPopup.selectedFile.split('/').pop();

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
                }
            }

            Button {
                text: "Browse"
                onClicked: fileDialog.open()
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
            model: [
                "420P - YUV420P (Planar)",
                "422P - YUV422P (Planar)", 
                "444P - YUV444P (Planar)",
                "YUYV - YUV422 (Packed)",
                "UYVY - YUV422 (Packed)",
                "NV12 - YUV420 (Semi-planar)",
                "NV21 - YUV420 (Semi-planar)"
            ]
            Layout.fillWidth: true
            currentIndex: 0
            
            property var formatValues: ["420P", "422P", "444P", "YUYV", "UYVY", "NV12", "NV21"]
            
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
                    const filePath = fileDialog.selectedFile;
                    let width = isYUV ? parseInt(res[0]) : 1920;
                    let height = isYUV ? parseInt(res[1]) : 1080;
                    let fps = isYUV ? parseFloat(fpsInput.text) : 25.0;
                    let format = isYUV ? formatInput.formatValues[formatInput.currentIndex] : "AV_PIX_FMT_NONE";
                    console.log("Importing video:", filePath, "Width:", width, "Height:", height, "FPS:", fps, "Format:", format);
                    importPopup.videoImported(filePath, width, height, fps, format);
                    importPopup.close();
                }
            }
        }
    }
    }
    
    function autoSelectFormat(filename) {
        if (!isYUV) return;
        
        const lowerFilename = filename.toLowerCase();
        
        // Check for specific format indicators in filename
        if (lowerFilename.includes("420p") || lowerFilename.includes("yuv420p")) {
            formatInput.currentIndex = 0; // 420P
        } else if (lowerFilename.includes("422p") || lowerFilename.includes("yuv422p")) {
            formatInput.currentIndex = 1; // 422P
        } else if (lowerFilename.includes("444p") || lowerFilename.includes("yuv444p")) {
            formatInput.currentIndex = 2; // 444P
        } else if (lowerFilename.includes("yuyv")) {
            formatInput.currentIndex = 3; // YUYV
        } else if (lowerFilename.includes("uyvy")) {
            formatInput.currentIndex = 4; // UYVY
        } else if (lowerFilename.includes("nv12")) {
            formatInput.currentIndex = 5; // NV12
        } else if (lowerFilename.includes("nv21")) {
            formatInput.currentIndex = 6; // NV21
        } else {
            // Default to 420P for most common YUV files
            formatInput.currentIndex = 0;
        }
        
        console.log("Auto-selected format:", formatInput.displayText, "for file:", filename);
    }
}
