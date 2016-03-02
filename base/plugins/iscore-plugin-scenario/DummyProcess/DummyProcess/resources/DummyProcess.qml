import QtQuick 2.3
import QtQuick.Controls 1.2
Item {
    id: root
    objectName: "root"
    width: 100
    height: 100

    Text {
        id: aText
        color: "black"
        anchors.fill: parent.Center
    }

    function makeText()
    {
        var text = "";
        var possible = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

        for( var i=0; i < 5; i++ )
            text += possible.charAt(Math.floor(Math.random() * possible.length));

        return text;
    }

    MouseArea
    {
        id: titi
        objectName: "input"
        anchors.fill: parent
        onClicked: aText.text = makeText()
    }
}
