import QtQuick 2.6
import QtQuick.Controls 1.5
import QtQuick.Layouts 1.3

Item {
    id: theroot

    property alias model : listView1.model
    property alias address_field: address_field
    property alias play_button: playButton
    property alias stop_button: stopButton
    property alias pause_button: pauseButton
    property alias reconnect_button: recoButton

    signal itemClicked(int index)

    ColumnLayout {
        x: 5
        y: 64
        Layout.alignment: Qt.AlignHCenter | Qt.AlignBottom
        ListView {
            id: listView1
            width: 400
            height: 350
            Layout.fillWidth: true
            clip: false

            model: ListModel{
                ListElement { name: "boo"; colorCode: "red"; }
                ListElement { name: "bah"; colorCode: "blu"; }
            }

            delegate: Button {
                id: row_button
                width: 200
                height: 40
                text: name
                antialiasing: false

                onClicked: theroot.itemClicked(index)
            }
        }

        TextField {
            id: address_field
            Layout.fillHeight: false
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
            Layout.fillWidth: true
            placeholderText: "ws://127.0.0.1:10212"
            text: "ws://147.210.128.72:10212"
        }

        RowLayout {
            id: rowLayout1
            width: 100
            height: 100
            Layout.fillHeight: true
            Layout.columnSpan: 1
            Layout.rowSpan: 1
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
            Layout.fillWidth: true

            Button {
                id: stopButton
                text: qsTr("Stop")
            }

            Button {
                id: pauseButton
                text: qsTr("Pause")
            }

            Button {
                id: playButton
                text: qsTr("Play")
            }

            Button {
                id: recoButton
                text: qsTr("Reconnect")
            }
        }

    }



}
