import QtQuick 2.0
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.3

Rectangle
{
    property alias textField: field
    property alias label: label
    property alias color: widg.color
    id: widg
    width: 212
    height: 86
    ColumnLayout {
        Text
        {
            id: label
            text: "default"
            Layout.preferredHeight: 18
            Layout.preferredWidth: 212
            font.pointSize: 12
            textFormat: Text.PlainText
            verticalAlignment: Text.AlignBottom
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
