import QtQuick 2.11
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

        Rectangle
        {
            height: 75
            width: theView.width
            border.color: "grey"
            id: delegateLayout

            MouseArea {
                id: mouseArea
                anchors.fill: delegateLayout
                drag.target: draggable
            }

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

    ListView {
        id: theView
        model: factoriesModel

        highlightFollowsCurrentItem: true
        anchors.fill: parent
        clip: true
        delegate: nameDelegate
        spacing: 5
    }
}
