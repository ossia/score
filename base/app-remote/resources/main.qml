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

    MainForm {
        id: form
        anchors.fill: parent
        onItemClicked: rootWindow.itemClicked(index)
        onPlay: rootWindow.play()
        onPause: rootWindow.pause()
        onStop: rootWindow.stop()
    }



}
