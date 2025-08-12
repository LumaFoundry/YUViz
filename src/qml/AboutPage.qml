import QtQuick.Window 6.0
import QtQuick.Controls 6.0
import QtQuick 6.0
import QtQuick.Layouts 1.15

Dialog {
    id: aboutDialog
    // Shown only on non-macOS platforms (macOS uses native About panel)
    visible: Qt.platform.os !== "osx"
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
                horizontalAlignment: Text.AlignHCenter
                width: parent.width
            }
            Column {
                width: parent.width
                spacing: Theme.spacingSmall
                Text {
                    text: "Version: " + (typeof APP_VERSION !== 'undefined' && APP_VERSION !== "" ? APP_VERSION : "DEV")
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.textColor
                    horizontalAlignment: Text.AlignHCenter
                    width: parent.width
                }
                Text {
                    text: "Built: " + BUILD_DATE
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.textColor
                    horizontalAlignment: Text.AlignHCenter
                    width: parent.width
                }
                Text {
                    text: "<a href='https://github.com/LokiW-03/qt6-videoplayer'>Source: GitHub Repository</a>"
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.accentColor
                    textFormat: Text.RichText
                    horizontalAlignment: Text.AlignHCenter
                    width: parent.width
                    onLinkActivated: Qt.openUrlExternally(link)
                }
            }
        }
    }
}
