import QtQuick 2.0
import QtQuick.Controls 2.0

UICheckBoxForm
{
    signal valueChange(bool val)

    onPositionChanged: { valueChange(position); }
}
