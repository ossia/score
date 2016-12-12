import QtQuick 2.0
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.3

Rectangle
{
    id: widg
    property alias slider: slider
    property alias label: label
    property alias color: widg.color
    width: 154
    height: 231

    ColumnLayout {
        width: 154
        height: 231
    Text
    {
        id: label
        x: 0
        y: 0
        width: 212
        height: 18
        text: "default"
        Layout.fillWidth: true
        font.pointSize: 12
        textFormat: Text.PlainText
        verticalAlignment: Text.AlignBottom
    }

    Slider
    {
        id: slider
        x: 86
        y: 24
        width: 40
        height: 162
        Layout.fillWidth: true
        orientation: Qt.Vertical
    }
    }
}
