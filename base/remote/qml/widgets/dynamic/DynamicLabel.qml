import QtQuick 2.7
import QtQuick.Controls 2.4

DynamicLabelForm
{
    id: widg
    signal addressChanged(string addr)

    property alias dropper: dropper
    AddressDrop
    {
        id: dropper
        item: widg
    }
}
