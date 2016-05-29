import QtQuick 2.6
import QtQuick.Controls 1.5
import QtQuick.Layouts 1.3

Item {
    id: theroot
    width: 640
    height: 480

    property alias model : listView1.model
    signal itemClicked(int index)
    signal play
    signal pause
    signal stop

    ColumnLayout {


        ListView {
            id: listView1
            x: 0
            y: 0
            width: 200
            height: 200

            model: ListModel{
                ListElement { name: "boo"; colorCode: "red"; }
                ListElement { name: "bah"; colorCode: "blu"; }
            }

            delegate: Item {
                x: 5
                width: 80
                height: 40
                Row {
                    id: row1
                    spacing: 10
                    Rectangle {
                        width: 40
                        height: 40
                        color: "red"
                        MouseArea
                        {
                            anchors.fill: parent
                            onClicked: itemClicked(index)
                        }

                    }

                    Text {
                        text: name
                        anchors.verticalCenter: parent.verticalCenter
                        font.bold: true
                    }
                }
            }
        }


        RowLayout
        {

            Button {
                id: playButton
                text: qsTr("Play")
                onClicked: theroot.play();
            }

            Button {
                id: pauseButton
                text: qsTr("Pause")
                onClicked: theroot.pause();
            }

            Button {
                id: stopButton
                text: qsTr("Stop")
                onClicked: theroot.stop();
            }
        }

    }


}
