import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: nodeItem

    property var node: null           // Process::ProcessModel*
    property var editContext: null     // EditJsContext*
    property var patcherRoot: null

    x: node ? node.position.x : 0
    y: node ? node.position.y : 0
    width: Math.max(180, contentColumn.width)
    height: contentColumn.height

    property bool isSelected: false
    property bool isHovered: false
    property var nodeInfo: node && editContext ? editContext.processInfo(node) : ({})
    property string nodeName: {
        if (!node) return ""
        return node.prettyName() || nodeInfo.name || "Process"
    }
    property string nodeCategory: nodeInfo.category || ""
    property bool showPreview: node && editContext ? editContext.hasGfxPreview(node) : false

    Column {
        id: contentColumn
        width: Math.max(180, implicitWidth)

        // Title bar
        Rectangle {
            id: titleBar
            width: parent.width
            height: 24
            radius: 4
            color: patcherRoot.categoryColor(nodeCategory)

            // Round only top corners
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: parent.radius
                color: parent.color
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 6
                anchors.rightMargin: 6
                spacing: 4

                Text {
                    text: nodeName
                    color: "white"
                    font.pixelSize: 11
                    font.bold: true
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }

            // Drag to move
            MouseArea {
                id: dragArea
                anchors.fill: parent
                property point pressPos
                property bool isDragging: false

                onPressed: (mouse) => {
                    pressPos = Qt.point(mouse.x, mouse.y)
                    isDragging = false
                    // Select this node
                    if (editContext) {
                        // TODO: proper selection through SelectionDispatcher
                    }
                    mouse.accepted = true
                }
                onPositionChanged: (mouse) => {
                    if (pressed) {
                        isDragging = true
                        var dx = mouse.x - pressPos.x
                        var dy = mouse.y - pressPos.y
                        var newPos = Qt.point(nodeItem.x + dx, nodeItem.y + dy)
                        if (node) node.position = newPos
                    }
                }
                onReleased: (mouse) => {
                    if (isDragging && editContext && node) {
                        editContext.moveNode(node, node.position)
                    }
                    isDragging = false
                }
            }
        }

        // Node body
        Rectangle {
            id: body
            width: parent.width
            height: portsArea.height + 8
            color: isSelected ? "#3a3a5a" : (isHovered ? "#2e2e48" : "#252540")
            border.color: isSelected ? "#6a6aaa" : (isHovered ? "#4a4a6a" : "#3a3a5a")
            border.width: isSelected ? 2 : 1

            // Round only bottom corners
            radius: 4
            Rectangle {
                anchors.top: parent.top
                width: parent.width
                height: parent.radius
                color: parent.color
            }

            Item {
                id: portsArea
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.topMargin: 4
                height: Math.max(inletsColumn.height, outletsColumn.height)
                        + (showPreview ? previewLoader.height : 0)

                // GFX Preview
                Loader {
                    id: previewLoader
                    active: showPreview
                    width: parent.width
                    height: active ? (parent.width * 9 / 16) : 0
                    anchors.top: parent.top

                    sourceComponent: NodePreview {
                        node: nodeItem.node
                    }
                }

                // Inlets (left side)
                Column {
                    id: inletsColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: previewLoader.bottom

                    Repeater {
                        model: node ? node.inletsList() : []
                        delegate: PortItem {
                            port: modelData
                            isInlet: true
                            nodeWidth: body.width
                            editContext: nodeItem.editContext
                            patcherRoot: nodeItem.patcherRoot
                            parentNode: nodeItem
                        }
                    }
                }

                // Outlets (right side, same Y as inlets)
                Column {
                    id: outletsColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: previewLoader.bottom

                    Repeater {
                        model: node ? node.outletsList() : []
                        delegate: PortItem {
                            port: modelData
                            isInlet: false
                            nodeWidth: body.width
                            editContext: nodeItem.editContext
                            patcherRoot: nodeItem.patcherRoot
                            parentNode: nodeItem
                        }
                    }
                }
            }

            // TODO: Execution progress bar - needs playPercentage property exposed
        }
    }

    // Hover detection
    HoverHandler {
        onHoveredChanged: {
            isHovered = hovered
        }
    }
}
