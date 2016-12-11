import QtQuick 2.0
import QtQuick.Controls 2.0

UILineEditForm
{
    signal textChange(string txt)

    onEditingFinished: { textChange(text); }
}
