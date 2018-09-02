import QtQuick 2.11
import QtQuick.Controls 2.4

DynamicHSliderForm
{
    id: widg
    signal valueChange(real val)
    signal addressChanged(string addr)

    slider.onPositionChanged: { valueChange(slider.from + slider.position * (slider.to - slider.from)); }

    property alias dropper: dropper
    AddressDrop
    {
        id: dropper
        item: widg
    }
}
