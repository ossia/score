import QtQuick 2.0
import QtQuick.Controls 2.0

DynamicVSliderForm
{
    signal valueChange(real val)
    signal addressChanged(string addr)

    slider.onPositionChanged: { valueChange(slider.from + slider.position * (slider.to - slider.from)); }

    id: widg
    property alias dropper: dropper
    AddressDrop
    {
        id: dropper
        item: widg
    }
}
