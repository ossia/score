import QtQuick 2.11
import QtQuick.Controls 1.4
import QtQuick.Extras 1.4

Text
{
    z:10
    id: label
    objectName: "addressLabel"
    text: "default"
    font.pointSize: 12
    textFormat: Text.PlainText
    verticalAlignment: Text.AlignBottom

    signal removeMe()

    MouseArea {
        id: mouseArea
        anchors.fill: label
        drag.target: widg // Defined in the parent of this object
        preventStealing: true

        Timer {
            id: pressAndHoldTimer
            interval: 300
            onTriggered: pieMenu.popup(mouseArea.mouseX, mouseArea.mouseY);
        }

        onPressed: pressAndHoldTimer.start()
        onMouseXChanged: {pressAndHoldTimer.stop(); pressAndHoldTimer.start()}
        onMouseYChanged: {pressAndHoldTimer.stop(); pressAndHoldTimer.start()}
        onReleased: pressAndHoldTimer.stop();
    }

    PieMenu {
        id: pieMenu

        triggerMode: TriggerMode.TriggerOnRelease

        MenuItem {
            iconName: "edit-delete"
            text: "Remove"
            onTriggered: label.removeMe()
        }
    }
}
