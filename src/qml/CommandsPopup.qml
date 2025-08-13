import QtQuick.Window 6.0
import QtQuick.Controls.Basic 6.0
import QtQuick 6.0
import QtQuick.Layouts 1.15

Window {
    id: commandsDialog
    title: "Keyboard Shortcuts"
    modality: Qt.ApplicationModal
    flags: Qt.Dialog
    width: 450
    height: 300

    color: Theme.surfaceColor

    ColumnLayout {
        anchors.fill: parent

        Rectangle {
            color: Theme.surfaceColor
            height: 40
            radius: Theme.cornerRadius
            Layout.fillWidth: true

            Text {
                anchors.centerIn: parent
                text: "Keyboard Shortcuts"
                font.bold: true
                font.pixelSize: Theme.fontSizeNormal
                color: Theme.textColor
            }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: Theme.spacingLarge
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
                            font.family: Theme.fontFamily
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
                            font.family: Theme.fontFamily
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
                            font.family: Theme.fontFamily
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
                            font.family: Theme.fontFamily
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
                            font.family: Theme.fontFamily
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
                            font.family: Theme.fontFamily
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
                            font.family: Theme.fontFamily
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

        Rectangle {
            color: Theme.surfaceColor
            height: 40
            radius: Theme.cornerRadius
            Layout.fillWidth: true

            Button {
                text: "OK"
                anchors.centerIn: parent
                onClicked: commandsDialog.close()
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
    }
}
