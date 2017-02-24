import QtQuick 2.0
import QtQuick.Controls 2.0

DynamicButtonForm
{
    id: widg
    signal addressChanged(string addr)
    property alias dropper: dropper
    signal clicked
    AddressDrop
    {
        id: dropper
        item: widg
    }

    Component.onCompleted: button.clicked.connect(clicked)
}
