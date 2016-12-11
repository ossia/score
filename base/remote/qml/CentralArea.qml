import QtQuick 2.0
import QtQuick.Layouts 1.3
import QtQuick.Controls 1.4

CentralAreaForm {
    Layout.fillWidth: true
    Layout.fillHeight: true


    ScrollView
    {
        anchors.fill: parent
        frameVisible: true
        horizontalScrollBarPolicy: Qt.ScrollBarAlwaysOn
        verticalScrollBarPolicy: Qt.ScrollBarAlwaysOn

        Rectangle {
            id: centralItem
            objectName: "centralItem"

            signal createObject(string objname, real x, real y)
            signal createAddress(string objname, real x, real y)

            width:1000
            height:1000
            implicitWidth:1000
            implicitHeight:1000
        }

        width:2000
        height:2000
    }

    DropArea {
        anchors.fill: parent
        keys: ["iscore/x-remote-widget", "iscore/x-remote-address"]
        onEntered: {
            centralItem.color = "#FCC"
        }
        onExited: {
            centralItem.color = "#EEE"
        }
        onDropped: {
            // Create a component
            var drop_fmt = drop.formats[0];
            var drop_text = drop.getDataAsString(drop_fmt);
            if(drop_fmt === "iscore/x-remote-widget")
            {
                centralItem.createObject(drop_text, drop.x, drop.y);
            }
            else if(drop_fmt === "iscore/x-remote-address")
            {
                centralItem.createAddress(drop_text, drop.x, drop.y);
            }

            drop.acceptProposedAction();
        }
    }
}
