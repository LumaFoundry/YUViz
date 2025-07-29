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
    property string psnrInfo: ""
    property alias diffVideoWindow: diffVideoWindow

    Connections {
        target: compareController
        function onPsnrChanged(psnrInfo) {
            diffWindow.psnrInfo = psnrInfo;
        }
    }

    DiffWindow {
        id: diffVideoWindow
        objectName: "diffWindow"
        anchors.fill: parent

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
