import QtQuick 6.0
import QtQuick.Controls.Basic 6.0
import QtQuick.Layouts 6.0
import QtQuick.Dialogs 6.2

Popup {
    id: importPopup
    width: 420
    height: 360
    modal: true
    focus: true
    clip: true
    x: (parent ? (parent.width - width) / 2 : 0)
    y: (parent ? (parent.height - height) / 2 : 0)

    property string selectedFile: ""
    property bool isYUV: selectedFile.toLowerCase().endsWith(".yuv")
    property var mainWindow
    signal accepted

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
                onAccepted: {
                    importPopup.selectedFile = selectedFile.toString().replace("file://", "");
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
                text: "Load"
                enabled: !isYUV || filePathInput.text !== "" && resolutionInput.text.match(/^\d+x\d+$/) && !isNaN(parseFloat(fpsInput.text))
                onClicked: {
                    const res = resolutionInput.text.split("x");
                    const path = fileDialog.selectedFile;
                    videoLoader.loadVideo(path, parseInt(res[0]), parseInt(res[1]), parseFloat(fpsInput.text));
                    importPopup.close();
                    importPopup.accepted();
                }
            }
        }
    }
}
