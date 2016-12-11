import QtQuick 2.0
import QtQuick.Controls 2.0

HSliderForm
{
    signal valueChange(real val)

    onPositionChanged: { valueChange(from + position * (to - from)); }
}
