import QtQuick 2.6
import QtQuick.Controls 1.5
import QtQuick.Dialogs 1.2

ApplicationWindow {
    visible: true
    width: 640
    height: 480
    title: qsTr("i-score-remote")

    id: rootWindow
    objectName: "Application"

    property alias model : form.model
    signal itemClicked(int index)
    signal play
    signal pause
    signal stop
    signal addressChanged(string address)

    MainForm {
        id: form
        anchors.fill: parent
        onItemClicked: { rootWindow.itemClicked(index); }
        play_button.onClicked: rootWindow.play()
        pause_button.onClicked: rootWindow.pause()
        stop_button.onClicked: rootWindow.stop()
        address_field.onEditingFinished: addressChanged(address_field.text)
        reconnect_button.onClicked: addressChanged(address_field.text)
    }



}
