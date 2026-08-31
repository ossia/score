import QtQuick
import QtQuick.Shapes

Shape {
    id: cableItem

    property var cable: null          // Process::Cable*
    property var editContext: null

    // These get resolved at render time
    property var sourcePort: editContext ? editContext.cableSource(cable) : null
    property var sinkPort: editContext ? editContext.cableSink(cable) : null
    property int dataType: sourcePort ? editContext.portType(sourcePort) : 0
    property bool isSelected: false

    // Source and target positions - updated via timer
    property real sx: 0
    property real sy: 0
    property real tx: 0
    property real ty: 0

    // Compute positions from port locations
    Timer {
        interval: 16  // ~60fps
        running: true
        repeat: true
        onTriggered: updatePositions()
    }

    Component.onCompleted: updatePositions()

    function updatePositions() {
        // Find source and target processes to compute positions
        if (!editContext || !cable) return

        var srcProc = editContext.cableSourceProcess(cable)
        var snkProc = editContext.cableSinkProcess(cable)
        if (!srcProc || !snkProc) return

        // Source port position: right edge of source node
        var srcPos = srcProc.position
        var srcSize = srcProc.size
        sx = srcPos.x + Math.max(180, srcSize.width)
        sy = srcPos.y + 36  // title height + some offset

        // Sink port position: left edge of sink node
        var snkPos = snkProc.position
        tx = snkPos.x
        ty = snkPos.y + 36

        // Try to find the correct port index for better positioning
        if (sourcePort) {
            var srcIdx = findPortIndex(srcProc, sourcePort, false)
            sy = srcPos.y + 28 + srcIdx * 16 + 8
        }
        if (sinkPort) {
            var snkIdx = findPortIndex(snkProc, sinkPort, true)
            ty = snkPos.y + 28 + snkIdx * 16 + 8
        }
    }

    function findPortIndex(proc, port, isInlet) {
        if (!proc) return 0
        var ports = isInlet ? proc.inletsList() : proc.outletsList()
        if (!ports) return 0
        for (var i = 0; i < ports.length; i++) {
            if (ports[i] === port) return i
        }
        return 0
    }

    function portTypeColor(type) {
        switch (type) {
        case 0: return "#4CAF50"  // Value
        case 1: return "#FF9800"  // Audio
        case 2: return "#2196F3"  // MIDI
        case 3: return "#9C27B0"  // Texture
        case 4: return "#F44336"  // Geometry
        default: return "#888888"
        }
    }

    ShapePath {
        strokeColor: isSelected ? "#ffffff" : portTypeColor(dataType)
        strokeWidth: isSelected ? 3 : 2
        fillColor: "transparent"
        capStyle: ShapePath.RoundCap

        startX: sx
        startY: sy

        PathCubic {
            property real ctrlDist: Math.max(50, Math.abs(tx - sx) * 0.4)
            control1X: sx + ctrlDist
            control1Y: sy
            control2X: tx - ctrlDist
            control2Y: ty
            x: tx
            y: ty
        }
    }

    // Click to select
    MouseArea {
        anchors.fill: parent
        onClicked: {
            isSelected = !isSelected
        }
    }
}
