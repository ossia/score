import QtQuick
import QtQuick.Controls
import QtQuick.Shapes

Item {
    id: root

    // Injected from C++
    // patcher: PatcherContext
    // Score: EditJsContext

    property color backgroundColor: "#1a1a2e"
    property real zoomLevel: patcher ? patcher.zoom : 1.0
    property point panOffset: patcher ? patcher.panOffset : Qt.point(0, 0)
    property var selectedNodes: ({})
    property var selectedCables: ({})

    // Canvas background
    Rectangle {
        anchors.fill: parent
        color: backgroundColor
    }

    // Grid pattern
    Canvas {
        id: gridCanvas
        anchors.fill: parent
        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.strokeStyle = "#2a2a4e"
            ctx.lineWidth = 1

            var gridSize = 20 * zoomLevel
            var ox = panOffset.x % gridSize
            var oy = panOffset.y % gridSize

            ctx.beginPath()
            for (var x = ox; x < width; x += gridSize) {
                ctx.moveTo(x, 0)
                ctx.lineTo(x, height)
            }
            for (var y = oy; y < height; y += gridSize) {
                ctx.moveTo(0, y)
                ctx.lineTo(width, y)
            }
            ctx.stroke()
        }
    }

    // Main canvas container - nodes and cables
    Item {
        id: nodeContainer
        x: panOffset.x
        y: panOffset.y
        transform: Scale {
            origin.x: 0
            origin.y: 0
            xScale: zoomLevel
            yScale: zoomLevel
        }

        // Cables layer (behind nodes)
        Repeater {
            id: cableRepeater
            model: patcher ? patcher.cables : []

            CableItem {
                cable: modelData
                editContext: Score
            }
        }

        // Nodes layer
        Repeater {
            id: nodeRepeater
            model: patcher ? patcher.nodes : []

            NodeItem {
                node: modelData
                editContext: Score
                patcherRoot: root
            }
        }
    }

    // Connection drag overlay (must be after nodeContainer so it's on top)
    ConnectionDrag {
        id: connectionDrag
        nodeContainer: nodeContainer
        editContext: Score
    }

    // Pan with middle mouse button or space+drag
    MouseArea {
        id: panArea
        anchors.fill: parent
        acceptedButtons: Qt.MiddleButton
        property point lastPos

        onPressed: (mouse) => {
            lastPos = Qt.point(mouse.x, mouse.y)
        }
        onPositionChanged: (mouse) => {
            if (pressed) {
                var dx = mouse.x - lastPos.x
                var dy = mouse.y - lastPos.y
                if (patcher) {
                    patcher.panOffset = Qt.point(
                        patcher.panOffset.x + dx,
                        patcher.panOffset.y + dy)
                }
                lastPos = Qt.point(mouse.x, mouse.y)
                gridCanvas.requestPaint()
            }
        }
    }

    // Zoom with scroll wheel
    WheelHandler {
        onWheel: (event) => {
            var factor = event.angleDelta.y > 0 ? 1.1 : 0.9
            var newZoom = Math.max(0.1, Math.min(5.0, zoomLevel * factor))
            if (patcher)
                patcher.zoom = newZoom
            gridCanvas.requestPaint()
        }
    }

    // Keyboard shortcuts
    focus: true
    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Delete || event.key === Qt.Key_Backspace) {
            // Delete selected items
            if (Score) {
                Score.startMacro()
                for (var id in selectedCables) {
                    Score.remove(selectedCables[id])
                }
                for (var nid in selectedNodes) {
                    Score.remove(selectedNodes[nid])
                }
                Score.endMacro()
                selectedNodes = {}
                selectedCables = {}
            }
            event.accepted = true
        }
        else if (event.key === Qt.Key_Z && (event.modifiers & Qt.ControlModifier)) {
            if (Score) Score.undo()
            event.accepted = true
        }
        else if (event.key === Qt.Key_Y && (event.modifiers & Qt.ControlModifier)) {
            if (Score) Score.redo()
            event.accepted = true
        }
    }

    // Right-click context menu for creating nodes
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        onClicked: (mouse) => {
            contextMenu.x = mouse.x
            contextMenu.y = mouse.y
            contextMenu.open()
        }
    }

    ProcessPalette {
        id: contextMenu
        editContext: Score
        patcherRoot: root
        nodeContainer: nodeContainer
        zoomLevel: root.zoomLevel
        panOffset: root.panOffset
    }

    // Drop area for process drops from the library
    DropArea {
        anchors.fill: parent
        onDropped: (drop) => {
            if (!Score) return
            var pos = mapToItem(nodeContainer, drop.x, drop.y)
            // The drop text contains the process key
            if (drop.text) {
                var interval = patcher ? patcher.container : null
                if (interval) {
                    var proc = Score.createProcess(interval, drop.text, "")
                    if (proc) {
                        Score.moveNode(proc, Qt.point(pos.x / zoomLevel, pos.y / zoomLevel))
                    }
                }
            }
        }
    }

    // Cable drag functions (called by PortItem)
    function startCableDrag(port, isInlet, portCircle, color) {
        var pos = portCircle.mapToItem(root, portCircle.width / 2, portCircle.height / 2)
        connectionDrag.startDrag(port, isInlet, Qt.point(pos.x, pos.y), color)
    }

    function updateCableDrag(pos) {
        connectionDrag.updateDrag(pos)
    }

    function finishCableDrag(pos) {
        connectionDrag.finishDrag(pos)
    }

    function endCableDrag() {
        connectionDrag.cancelDrag()
    }

    // Helper functions
    function portTypeColor(portType) {
        switch (portType) {
        case 0: return "#4CAF50"  // Message/Value - green
        case 1: return "#FF9800"  // Audio - orange
        case 2: return "#2196F3"  // MIDI - blue
        case 3: return "#9C27B0"  // Texture - purple
        case 4: return "#F44336"  // Geometry - red
        default: return "#888888"
        }
    }

    function categoryColor(category) {
        switch (category) {
        case "Audio": return "#c0392b"
        case "Visuals": return "#8e44ad"
        case "Midi": return "#2980b9"
        case "Control": return "#27ae60"
        case "Script": return "#f39c12"
        case "Automations": return "#e67e22"
        case "Structure": return "#2c3e50"
        default: return "#34495e"
        }
    }
}
