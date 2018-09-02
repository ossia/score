import QtQuick 2.11
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.3
Item {
    objectName: "NodeTree"
    TreeView {
        id: nodeView
        model: nodesModel
        objectName: "nodeView"
        anchors.fill: parent


        TableViewColumn {
            title: "Address"
            role: "address"
            width: 150
        }
        /*
        TableViewColumn {
            title: "Value"
            role: "value"
            width: 60
        }
        */

        Component {
            id: treeDelegate

            Item
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
                    anchors.verticalCenter: parent.verticalCenter
                    color: "#000000"
                    elide: styleData.elideMode
                    text: styleData.value
                    font.pointSize:  12

                    Item {
                        id: draggable
                        width: 0
                        height: 0
                        anchors.fill: parent

                        Drag.active: mouseArea.drag.active
                        Drag.hotSpot.x: 0
                        Drag.hotSpot.y: 0
                        Drag.onDragStarted: {
                            console.log( nodesModel.nodeToAddressString(styleData.index))
                            console.log("imma dragin")
                        }

                        Drag.mimeData: {
                            "iscore/x-remote-address": nodesModel.nodeToAddressString(styleData.index)
                        }
                        Drag.dragType: Drag.Automatic
                    }
                }

            }
        }

        itemDelegate: treeDelegate

        rowDelegate: Rectangle {
            color: ( styleData.alternate ) ? "#ffffff" : "#fafaff"
            height: 25
        }
    }

}
