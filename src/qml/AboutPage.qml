import QtQuick.Window 6.0
import QtQuick.Controls.Basic 6.0
import QtQuick 6.0
import QtQuick.Layouts 1.15

Dialog {
    id: aboutDialog
    title: "About"
    modal: true
    standardButtons: Dialog.Ok
    width: 450
    height: 250

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
            text: "About"
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
            onClicked: aboutDialog.accept()
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

            Text {
                text: APP_NAME
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
                        text: "Version:"
                        font.family: "sans-serif"
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.accentColor
                        width: 120
                    }
                    Text {
                        text: typeof APP_VERSION !== 'undefined' && APP_VERSION !== "" ? APP_VERSION : "DEV"
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.textColor
                    }
                }

                Row {
                    width: parent.width

                    Text {
                        text: "Built:"
                        font.family: "sans-serif"
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.accentColor
                        width: 120
                    }
                    Text {
                        text: BUILD_DATE
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.textColor
                    }
                }
                Row {
                    width: parent.width

                    Text {
                        text: "Source:"
                        font.family: "sans-serif"
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.accentColor
                        width: 120
                    }
                    Text {
                        text: "<a href='https://github.com/LokiW-03/qt6-videoplayer'>GitHub Repository</a>"
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.accentColor
                        textFormat: Text.RichText
                        onLinkActivated: Qt.openUrlExternally(link)
                    }
                }
            }
        }
    }
}
