import QtQuick 2.0
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.3

Rectangle
{
    property alias button: button
    property alias label: label
    id: widg
    width: 212
    height: 86
    color: "#00000000"
    Drag.dragType: Drag.Internal

    ColumnLayout {
        AddressLabel
        {
            id: label
            Layout.preferredHeight: 18
            Layout.preferredWidth: 212
        }
        Button
        {
            id: button
            text: qsTr("Button")
            implicitWidth: 150
            implicitHeight: 40
        }
    }

}
