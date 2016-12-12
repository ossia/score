import QtQuick 2.0
import QtQuick.Controls 2.0

DynamicSwitchForm
{
    signal valueChange(bool val)
    signal addressChanged(string addr)

    slider.onPositionChanged: { valueChange(position); }
    id: widg
    property alias dropper: dropper
    AddressDrop
    {
        id: dropper
        item: widg
    }
}
