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

    RowLayout
    {
        x: 5
        y: 8
        width: 627
        height: 464

        ListView {
            id: listView1
            y: 0
            width: 200
            height: 350

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
                            //onClicked: theroot.itemClicked(index)
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

        Item
        {
            Item
            {
                Layout.fillWidth: true
                Layout.fillHeight: true
            }

            RowLayout {
                id: rowLayout1
                width: 100
                height: 100

                Button {
                    id: stopButton
                    text: qsTr("Stop")
                    //onClicked: theroot.stop();
                }

                Button {
                    id: pauseButton
                    text: qsTr("Pause")
                    //onClicked: theroot.pause();
                }

                Button {
                    id: playButton
                    text: qsTr("Play")
                    //onClicked: theroot.play();
                }
            }
        }





    }



}
