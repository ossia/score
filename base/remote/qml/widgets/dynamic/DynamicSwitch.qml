import QtQuick 2.11
import QtQuick.Controls 2.4

DynamicSwitchForm
{
    signal valueChange(bool val)
    signal addressChanged(string addr)

    slider.onPositionChanged: { valueChange(slider.position); }
    id: widg
    property alias dropper: dropper
    AddressDrop
    {
        id: dropper
        item: widg
    }
}
