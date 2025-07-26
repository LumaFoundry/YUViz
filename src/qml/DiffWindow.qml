import QtQuick 6.0
import QtQuick.Controls.Basic 6.0
import Window 1.0
import QtQuick.Layouts 1.15

Window {
    id: diffWindow
    width: 500
    height: 400
    visible: false
    title: "Diff Result"
    flags: Qt.Window

    property point dragPos
    property int leftVideoId: -1
    property int rightVideoId: -1
    property int diffVideoId: -1

    VideoWindow {
        id: diffVideoWindow
        objectName: "diffWindow"
        anchors.fill: parent
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
