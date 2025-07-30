import QtQuick 6.0
import QtQuick.Controls.Basic 6.0
import QtQuick.Layouts 1.15
import Theme 1.0

Popup {
    id: resolutionWarningPopup

    property int newWidth: 0
    property int newHeight: 0
    property int firstWidth: 0
    property int firstHeight: 0

    width: 500
    height: 180
    modal: true
    focus: true
    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    padding: 20

    background: Rectangle {
        color: Theme.surfaceColor
        radius: 6
        border.color: Theme.borderColor
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 15

        Text {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.textColor
            font.pixelSize: Theme.fontSizeNormal
            text:    "Resolution Mismatch Warning:\n\nThe newly added video has a resolution of " + newWidth + "x" + newHeight +
                        ", which is different from the first video's resolution of " + firstWidth + "x" + firstHeight +
                        ".\n\nThis may lead to unexpected behavior."
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 10

            Button {
                text: "OK"
                onClicked: resolutionWarningPopup.close()
            }
        }
    }
}