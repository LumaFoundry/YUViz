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
    property real newFps: 0.0
    property real firstFps: 0.0

    width: 500
    height: 220
    modal: true
    focus: true
    x: (parent ? (parent.width - width) / 2 : 0)
    y: (parent ? (parent.height - height) / 2 : 0)
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
            text: {
                let warningText = "Mismatch Warning:\n";
                const resMismatch = (newWidth !== firstWidth || newHeight !== firstHeight);
                const fpsMismatch = (firstFps !== newFps);

                if (resMismatch) {
                    warningText += "\n• The new video's resolution (" + newWidth + "x" + newHeight + ") "
                                 + "differs from the first video's (" + firstWidth + "x" + firstHeight + ").";
                }

                if (fpsMismatch) {
                    warningText += "\n• The new video's frame rate (" + newFps + " fps) "
                                 + "differs from the first video's (" + firstFps + " fps).";
                }

                warningText += "\n\nThis may lead to unexpected behavior.";
                return warningText;
            }
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
