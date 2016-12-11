import QtQuick 2.0
import QtQuick.Layouts 1.3

Item {
    Rectangle
    {
        anchors.fill: parent
        border.color: "black"
    }

    id: widgList

    Component {
        id: itemFiller
        Item { Layout.fillHeight: true }
    }

    Component {
        id: nameDelegate

        ColumnLayout
        {
            height:70
            id: delegateLayout

            MouseArea {
                id: mouseArea
                anchors.fill: text
                drag.target: draggable
            }

            Text {
                id: text
                text: prettyName;
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 16

                Component.onCompleted: {
                    var obj = exampleComponent.createObject(delegateLayout, {});
                    obj.enabled = false;
                    itemFiller.createObject(delegateLayout, {});
                }

                Item {
                    id: draggable
                    width: 0
                    height: 0
                    anchors.fill: parent

                    Drag.active: mouseArea.drag.active
                    Drag.hotSpot.x: 0
                    Drag.hotSpot.y: 0

                    Drag.mimeData: { "iscore/x-remote-widget": name }
                    Drag.dragType: Drag.Automatic
                }
            }

        }
    }

    ListView {
        model: factoriesModel

        anchors.fill: parent
        clip: true
        delegate: nameDelegate
    }
}
