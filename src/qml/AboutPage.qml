import QtQuick.Window 6.0
import QtQuick.Controls.Basic 6.0
import QtQuick 6.0
import QtQuick.Layouts 1.15

Window {
    id: aboutDialog
    // Shown only on non-macOS platforms (macOS uses native About panel)
    visible: Qt.platform.os !== "osx"
    title: "About"
    modality: Qt.ApplicationModal
    flags: Qt.Dialog
    width: 450
    height: 200

    color: Theme.surfaceColor

    Rectangle {
        anchors.fill: parent
        color: "transparent"
        radius: Theme.cornerRadius
        border.color: Theme.borderColor

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.spacingLarge
            spacing: 0

            // Header
            Rectangle {
                id: headerRect
                Layout.fillWidth: true
                height: 10
                color: Theme.surfaceColor
                radius: Theme.cornerRadius

                MouseArea {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    property point dragStartPos
                    property point windowStartPos

                    onPressed: function (mouse) {
                        dragStartPos = Qt.point(mouse.screenX, mouse.screenY);
                        windowStartPos = Qt.point(aboutDialog.x, aboutDialog.y);
                    }
                    onPositionChanged: function (mouse) {
                        if (mouse.buttons & Qt.LeftButton) {
                            var dx = mouse.screenX - dragStartPos.x;
                            var dy = mouse.screenY - dragStartPos.y;
                            aboutDialog.x = windowStartPos.x + dx;
                            aboutDialog.y = windowStartPos.y + dy;
                        }
                    }
                    cursorShape: Qt.OpenHandCursor
                }
            }

            // Content
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
                                font.family: Theme.fontFamily
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
                                font.family: Theme.fontFamily
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
                                font.family: Theme.fontFamily
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

            // Footer
            Rectangle {
                Layout.fillWidth: true
                height: 40
                color: Theme.surfaceColor
                radius: Theme.cornerRadius

                Button {
                    id: okButton
                    text: "OK"
                    anchors.centerIn: parent
                    onClicked: {
                        aboutDialog.close();
                        keyHandler.forceActiveFocus();
                    }
                    background: Rectangle {
                        color: okButton.hovered ? Theme.primaryColor : Theme.buttonColor
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
}
