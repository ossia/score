import QtQuick
import QtQuick.Shapes

// Pure visual component - no MouseArea. All mouse events are driven
// by PortItem which holds the mouse grab.
Item {
    id: connectionDrag

    property var nodeContainer: null
    property var editContext: null

    property bool isDragging: false
    property var sourcePort: null
    property bool sourceIsInlet: false
    property point startPos: Qt.point(0, 0)
    property point endPos: Qt.point(0, 0)
    property color cableColor: "#888888"

    anchors.fill: parent

    Shape {
        anchors.fill: parent
        visible: isDragging

        ShapePath {
            strokeColor: cableColor
            strokeWidth: 2
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            strokeStyle: ShapePath.DashLine
            dashPattern: [4, 4]

            startX: startPos.x
            startY: startPos.y

            PathCubic {
                property real ctrlDist: Math.max(30, Math.abs(endPos.x - startPos.x) * 0.4)
                control1X: sourceIsInlet ? (startPos.x - ctrlDist) : (startPos.x + ctrlDist)
                control1Y: startPos.y
                control2X: sourceIsInlet ? (endPos.x + ctrlDist) : (endPos.x - ctrlDist)
                control2Y: endPos.y
                x: endPos.x
                y: endPos.y
            }
        }
    }

    function startDrag(port, isInlet, pos, color) {
        sourcePort = port
        sourceIsInlet = isInlet
        startPos = pos
        endPos = pos
        cableColor = color
        isDragging = true
    }

    function updateDrag(pos) {
        endPos = pos
    }

    function finishDrag(pos) {
        if (!isDragging) return

        endPos = pos
        var targetPort = findPortAt(pos.x, pos.y)
        if (targetPort && targetPort !== sourcePort && editContext) {
            // Determine direction: source is where drag started
            // If source was an inlet, target must be an outlet (and vice versa)
            var srcIsInlet = sourcePort.isInlet()
            var tgtIsInlet = targetPort.isInlet()

            // Only connect if they're different types (inlet<->outlet)
            if (srcIsInlet !== tgtIsInlet) {
                var outlet = srcIsInlet ? targetPort : sourcePort
                var inlet = srcIsInlet ? sourcePort : targetPort
                editContext.createCable(outlet, inlet)
            }
        }
        cancelDrag()
    }

    function cancelDrag() {
        isDragging = false
        sourcePort = null
    }

    function findPortAt(mx, my) {
        if (!nodeContainer) return null
        var hitRadius = 20
        for (var i = 0; i < nodeContainer.children.length; i++) {
            var child = nodeContainer.children[i]
            if (!child) continue
            var result = findPortInChildren(child, mx, my, hitRadius, 0)
            if (result) return result
        }
        return null
    }

    function findPortInChildren(item, mx, my, hitRadius, depth) {
        if (!item) return null

        // Check if this item is a PortItem (has port property set and getGlobalCenter)
        if (item.port !== null && item.port !== undefined
                && typeof item.getGlobalCenter === "function") {
            // Port circle is at the edge: x=0 for inlets, x=nodeWidth for outlets
            var circleX = item.isInlet ? 0 : item.nodeWidth
            var circleY = item.height / 2
            var center = item.mapToItem(connectionDrag, circleX, circleY)
            var dx = center.x - mx
            var dy = center.y - my
            var dist = Math.sqrt(dx * dx + dy * dy)
            if (dist < hitRadius) {
                return item.port
            }
        }

        // Recurse into children
        if (!item.children) return null
        for (var i = 0; i < item.children.length; i++) {
            var result = findPortInChildren(item.children[i], mx, my, hitRadius, depth + 1)
            if (result) return result
        }
        return null
    }
}
