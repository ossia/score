import QtQuick 2.3
import QtQuick.Controls 1.2
Item {
    id: root
    width: 200
    height: 100

    Text {
        id: lol
        text: 'toto'
        color: "yellow"
    }

    Button {
        text: "Push Me"
        anchors.centerIn: parent
        onClicked: lol.text = "dudu"
    }
}
