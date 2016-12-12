import QtQuick 2.0
import QtQuick.Controls 2.0

DynamicButtonForm
{
    id: widg
    property alias dropper: dropper
    AddressDrop
    {
        id: dropper
        item: widg
    }
}
