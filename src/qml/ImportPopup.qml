import QtQuick 6.0
import QtQuick.Controls.Basic 6.0
import QtQuick.Layouts 6.0
import QtQuick.Dialogs 6.2

Popup {
    id: importPopup
    width: 420
    height: 420
    modal: true
    focus: true
    clip: true
    x: (parent ? (parent.width - width) / 2 : 0)
    y: (parent ? (parent.height - height) / 2 : 0)

    property string mode: "new"  // "new" or "add"
    property string selectedFile: ""
    property bool isYUV: selectedFile.toLowerCase().endsWith(".yuv")
    property var mainWindow
    signal videoImported(string filePath, int width, int height, double fps, string pixelFormat, bool add)
    signal accepted

    function openFileDialog() {
        fileDialog.open();
    }

    background: Rectangle {
        color: "white"
        radius: 6
        border.color: "#aaa"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 12

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
                        resolutionInput.text = match[1] + "x" + match[2];
                        fpsInput.text = match[3];
                    } else {
                        // Fallback: just try to extract resolution anywhere
                        const resMatch = file.match(/(\d{3,5})x(\d{3,5})/);
                        if (resMatch) {
                            resolutionInput.text = resMatch[0];
                        } else {
                            resolutionInput.text = "";
                        }
                        fpsInput.text = "";
                    }
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

        TextField {
            id: resolutionInput
            visible: isYUV
            placeholderText: "1920x1080"
            Layout.fillWidth: true
            onAccepted: {
                if (resolutionInput.text === "") {
                    resolutionInput.text = resolutionInput.placeholderText;
                }
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
            text: "Format"
            Layout.fillWidth: true
        }

        ComboBox {
            id: formatInput
            visible: isYUV
            model: ["420P", "422P", "444P"]
            Layout.fillWidth: true
            currentIndex: 0
            displayText: model[currentIndex]
        }

        Item {
            Layout.fillHeight: true
        }  // Spacer to push buttons to the bottom

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 10

            Button {
                text: "Cancel"
                onClicked: importPopup.close()
            }

            Button {
                text: mode === "add" ? "Add" : "Load"
                enabled: importPopup.selectedFile !== "" && (!isYUV || (filePathInput.text !== "" && resolutionInput.text.match(/^\d+x\d+$/) && !isNaN(parseFloat(fpsInput.text))))
                onClicked: {
                    const res = resolutionInput.text.split("x");
                    const filePath = fileDialog.selectedFile;
                    let width = isYUV ? parseInt(res[0]) : 0;
                    let height = isYUV ? parseInt(res[1]) : 0;
                    let fps = isYUV ? parseFloat(fpsInput.text) : 0;
                    let format = isYUV ? formatInput.currentText : "AV_PIX_FMT_NONE";
                    console.log("Importing video:", filePath, "Width:", width, "Height:", height, "FPS:", fps, "Format:", format);
                    let add = mode === "add";
                    importPopup.videoImported(filePath, width, height, fps, format, add);
                    importPopup.close();
                }
            }
        }
    }
}
