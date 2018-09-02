import QtQuick 2.11
import QtQuick.Controls 2.4

Item
{
    Text
    {
        x: 0
        y: 0
        width: 212
        height: 18
        text: qsTr("Slider")
        horizontalAlignment: Text.AlignHCenter
        font.pointSize: 12
        textFormat: Text.PlainText
        verticalAlignment: Text.AlignBottom
    }

    Slider
    {
        x: 15
        y: 24
        width: 182
        height: 40
        enabled: false
    }
}
