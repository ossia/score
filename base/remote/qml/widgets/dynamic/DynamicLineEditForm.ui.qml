import QtQuick 2.11
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.3

Rectangle
{
    property alias textField: field
    property alias label: label
    property alias color: widg.color

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

        TextField {
            id: field
            text: qsTr("Textedit")
            Layout.fillWidth: true

            implicitWidth: 150
            implicitHeight: 40
            width: 150
            height: 40
        }

    }

}
