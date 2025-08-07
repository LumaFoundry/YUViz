import QtQuick.Window 6.0
import QtQuick.Controls.Basic 6.0
import QtQuick 6.0
import QtQuick.Layouts 1.15

Dialog {
    id: commandsDialog
    title: "Keyboard Shortcuts"
    modal: true
    standardButtons: Dialog.Ok
    width: 450
    height: 300

    background: Rectangle {
        color: Theme.surfaceColor
        radius: Theme.cornerRadius
        border.color: Theme.borderColor
    }

    header: Rectangle {
        color: Theme.surfaceColor
        height: 40
        radius: Theme.cornerRadius

        Text {
            anchors.centerIn: parent
            text: "Keyboard Shortcuts"
            font.bold: true
            font.pixelSize: Theme.fontSizeNormal
            color: Theme.textColor
        }
    }

    footer: Rectangle {
        color: Theme.surfaceColor
        height: 40
        radius: Theme.cornerRadius

        Button {
            text: "OK"
            anchors.centerIn: parent
            onClicked: commandsDialog.accept()
            background: Rectangle {
                color: Theme.buttonColor
                radius: Theme.cornerRadius
                border.color: Theme.borderColor
            }
            contentItem: Text {
                text: parent.text
                color: Theme.textColor
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }
    }

    ScrollView {
        anchors.fill: parent
        anchors.margins: Theme.spacingLarge
        background: Rectangle {
            color: Theme.surfaceColor
        }

        Column {
            width: parent.width
            spacing: Theme.spacingNormal

            // Playback Controls
            Text {
                text: "Playback Controls"
                font.bold: true
                font.pixelSize: Theme.fontSizeNormal
                color: Theme.textColor
            }

            Column {
                width: parent.width
                spacing: Theme.spacingSmall

                Row {
                    width: parent.width

                    Text {
                        text: "Space"
                        font.family: "monospace"
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.accentColor
                        width: 120
                    }
                    Text {
                        text: "Play/Pause"
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.textColor
                    }
                }

                Row {
                    width: parent.width

                    Text {
                        text: "← →"
                        font.family: "monospace"
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.accentColor
                        width: 120
                    }
                    Text {
                        text: "Step backward/forward"
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.textColor
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: 1
                color: Theme.borderColor
                opacity: 0.3
            }

            // Display Controls
            Text {
                text: "Display Controls"
                font.bold: true
                font.pixelSize: Theme.fontSizeNormal
                color: Theme.textColor
            }

            Column {
                width: parent.width
                spacing: Theme.spacingSmall

                Row {
                    width: parent.width

                    Text {
                        text: "O"
                        font.family: "monospace"
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.accentColor
                        width: 120
                    }
                    Text {
                        text: "Toggle Display"
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.textColor
                    }
                }

                Row {
                    width: parent.width

                    Text {
                        text: "J"
                        font.family: "monospace"
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.accentColor
                        width: 120
                    }
                    Text {
                        text: "Jump to Frame"
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.textColor
                    }
                }

                Row {
                    width: parent.width

                    Text {
                        text: "Escape"
                        font.family: "monospace"
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.accentColor
                        width: 120
                    }
                    Text {
                        text: "Exit Fullscreen"
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.textColor
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: 1
                color: Theme.borderColor
                opacity: 0.3
            }

            // Zoom Controls
            Text {
                text: "Zoom Controls"
                font.bold: true
                font.pixelSize: Theme.fontSizeNormal
                color: Theme.textColor
            }

            Column {
                width: parent.width
                spacing: Theme.spacingSmall

                Row {
                    width: parent.width

                    Text {
                        text: "Ctrl + Wheel"
                        font.family: "monospace"
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.accentColor
                        width: 120
                    }
                    Text {
                        text: "Zoom in/out"
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.textColor
                    }
                }

                Row {
                    width: parent.width

                    Text {
                        text: "Ctrl + Drag"
                        font.family: "monospace"
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.accentColor
                        width: 120
                    }
                    Text {
                        text: "Draw rectangle to zoom"
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.textColor
                    }
                }
            }
        }
    }
}
