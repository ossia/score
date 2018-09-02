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
        text: qsTr("Button")
        horizontalAlignment: Text.AlignHCenter
        font.pointSize: 12
        textFormat: Text.PlainText
        verticalAlignment: Text.AlignBottom
    }

    Button
    {
        x: 22
        y: 24
        width: 168
        height: 40
        text: "Button"
    }
}
