import QtQuick 6.0
import QtQuick.Controls.Basic 6.0
import QtQuick.Controls 6.2
import QtQuick.Layouts 6.0
import Theme 1.0

Popup {
    id: jumpPopup
    width: Math.min(300, parent.width - 40)
    height: Math.min(180, parent.height - 40)
    modal: true
    focus: true
    clip: true
    x: (parent ? (parent.width - width) / 2 : 0)
    y: (parent ? (parent.height - height) / 2 : 0)

    property int maxFrames: 0
    property alias frameNumber: frameInput.text

    signal jumpToFrame(int frameNumber)

    function validateAndJump() {
        if (frameInput.text === "" || isNaN(parseInt(frameInput.text)) || parseInt(frameInput.text) < 0 || parseInt(frameInput.text) >= maxFrames) {
            errorLabel.visible = true;
            errorLabel.text = "Frame number must be between 0 and " + (maxFrames - 1);
            return;
        }
        errorLabel.text = "";
        jumpToFrame(parseInt(frameInput.text));
        jumpPopup.close();
        frameInput.text = "";
    }

    background: Rectangle {
        color: Theme.backgroundColor
        radius: 6
        border.color: Theme.borderColor
        border.width: 1
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Label {
            text: "Jump to Frame"
            font.pixelSize: 16
            font.bold: true
            color: Theme.textColor
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: "Frame:"
                color: Theme.textColor
            }

            TextField {
                id: frameInput
                Layout.fillWidth: true
                placeholderText: maxFrames > 0 ? "Enter frame number (0-" + (maxFrames - 1) + ")" : "Enter frame number"
                selectByMouse: true

                background: Rectangle {
                    color: Theme.inputBackgroundColor
                    border.color: frameInput.activeFocus ? Theme.accentColor : Theme.borderColor
                    border.width: 1
                    radius: 4
                }

                color: Theme.textColor

                Keys.onPressed: event => {
                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                        validateAndJump();
                        event.accepted = true;
                    } else if (event.key === Qt.Key_Escape) {
                        jumpPopup.close();
                        event.accepted = true;
                    }
                }
            }
        }

        Label {
            id: errorLabel
            text: ""
            color: "red"
            visible: true
            Layout.fillWidth: true
            Layout.preferredHeight: 16
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: 12
        }

        RowLayout {
            spacing: 16
            Layout.alignment: Qt.AlignHCenter

            Button {
                text: "Cancel"
                onClicked: jumpPopup.close()

                background: Rectangle {
                    color: parent.pressed ? Theme.buttonPressedColor : Theme.buttonColor
                    border.color: Theme.borderColor
                    border.width: 1
                    radius: 4
                }

                contentItem: Text {
                    text: parent.text
                    color: Theme.textColor
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Button {
                text: "OK"
                onClicked: validateAndJump()

                background: Rectangle {
                    color: parent.pressed ? Theme.accentPressedColor : Theme.accentColor
                    border.color: Theme.accentColor
                    border.width: 1
                    radius: 4
                }

                contentItem: Text {
                    text: parent.text
                    color: "white"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
}
