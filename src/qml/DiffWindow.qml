import QtQuick 6.0
import QtQuick.Controls.Basic 6.0
import DiffWindow 1.0
import QtQuick.Layouts 1.15

Window {
    id: diffWindow
    width: 500
    height: 400
    visible: false
    title: "Diff Result"
    flags: Qt.Window
    color: "black"

    property point dragPos
    property int leftVideoId: -1
    property int rightVideoId: -1
    property int diffVideoId: -1
    property string psnrInfo: compareController.psnrInfo
    property alias diffVideoWindow: diffVideoWindow

    DiffWindow {
        id: diffVideoWindow
        objectName: "diffWindow"
        anchors.fill: parent

        // Configuration menu
        Row {
            anchors.top: parent.top
            anchors.right: parent.right
            spacing: 8

            ComboBox {
                width: 0
                height: 40
                model: ["Don't remove this ComboBox"]
                indicator: Canvas {
                }
            }

            Button {
                id: menuButton
                width: 40
                height: 40
                text: "⚙"
                font.pixelSize: 30
                background: Rectangle {
                    color: "transparent"
                }
                contentItem: Text {
                    text: menuButton.text
                    font.pixelSize: menuButton.font.pixelSize
                    color: "white"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: menu.open()

                Menu {
                    id: menu
                    y: menuButton.height

                    MenuItem {
                        contentItem: ColumnLayout {
                            spacing: 8
                            width: 200

                            Text {
                                text: "Display Mode"
                                color: "black"
                                font.pixelSize: 12
                            }

                            ComboBox {
                                id: displayModeSelector
                                model: ["Grayscale", "Heatmap", "Binary"]
                                width: 180
                                currentIndex: diffVideoWindow.displayMode

                                onCurrentIndexChanged: {
                                    diffVideoWindow.displayMode = currentIndex;
                                    keyHandler.forceActiveFocus();
                                }

                                onActivated: keyHandler.forceActiveFocus()
                            }

                            Text {
                                text: "Diff Method"
                                color: "black"
                                font.pixelSize: 12
                            }

                            ComboBox {
                                id: diffMethodSelector
                                model: ["Direct Subtraction", "Squared Difference", "Normalized", "Absolute Difference"]
                                width: 180
                                currentIndex: diffVideoWindow.diffMethod

                                onCurrentIndexChanged: {
                                    diffVideoWindow.diffMethod = currentIndex;
                                    keyHandler.forceActiveFocus();
                                }

                                onActivated: keyHandler.forceActiveFocus()
                            }

                            Text {
                                text: "Multiplier"
                                color: "black"
                                font.pixelSize: 12
                            }

                            TextField {
                                id: multiplierInput
                                width: 180
                                text: diffVideoWindow.diffMultiplier.toString()
                                color: "black"
                                background: Rectangle {
                                    color: "transparent"
                                    border.color: "black"
                                    border.width: 1
                                }

                                onEditingFinished: {
                                    let value = parseFloat(text);
                                    if (!isNaN(value) && value > 0) {
                                        diffVideoWindow.diffMultiplier = value;
                                    } else {
                                        text = diffVideoWindow.diffMultiplier.toString();
                                    }
                                    keyHandler.forceActiveFocus();
                                }

                                onAccepted: keyHandler.forceActiveFocus()
                            }
                        }
                    }

                    MenuSeparator {
                    }

                    MenuItem {
                        text: "Close Diff Window"
                        onTriggered: diffWindow.close()
                    }
                }
            }
        }

        // OSD Overlay
        Rectangle {
            visible: diffVideoWindow.osdState > 0
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.margins: 10
            width: osdText.width + 20
            height: osdText.height + 20
            color: "black"
            opacity: 0.8
            radius: 5
            z: 100 // Ensure it's on top

            Text {
                id: osdText
                anchors.centerIn: parent
                color: "white"
                font.family: "monospace"
                font.pixelSize: 12
                text: {
                    var osdStr = diffWindow.psnrInfo;

                    if (diffVideoWindow.osdState === 1) {
                        // Basic info: PSNR
                        return osdStr;
                    } else if (diffVideoWindow.osdState === 2) {
                        // Detailed info: Could add later
                        return osdStr;
                    }
                    return "";
                }
            }
        }
    }

    MouseArea {
        onPressed: function (mouse) {
            dragPos = Qt.point(mouse.x, mouse.y);
        }
        onPositionChanged: function (mouse) {
            if (mouse.buttons & Qt.LeftButton) {
                diffWindow.x += mouse.x - dragPos.x;
                diffWindow.y += mouse.y - dragPos.y;
            }
        }
    }
}
