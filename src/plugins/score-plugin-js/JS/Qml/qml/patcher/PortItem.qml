import QtQuick

Item {
    id: portItem

    property var port: null           // Process::Port*
    property bool isInlet: true
    property real nodeWidth: 180
    property var editContext: null
    property var patcherRoot: null
    property var parentNode: null

    height: 16
    width: nodeWidth

    property int pType: port ? editContext.portType(port) : 0
    property string portName: port ? port.name : ""
    property color portColor: patcherRoot.portTypeColor(pType)

    // Port circle
    Rectangle {
        id: portCircle
        width: 10
        height: 10
        radius: 5
        color: portColor
        border.color: Qt.lighter(portColor, 1.3)
        border.width: 1.5
        anchors.verticalCenter: parent.verticalCenter
        x: isInlet ? -5 : (nodeWidth - 5)

        // Glow on hover
        Rectangle {
            anchors.centerIn: parent
            width: parent.width + 4
            height: parent.height + 4
            radius: width / 2
            color: "transparent"
            border.color: portColor
            border.width: 1
            opacity: portMouseArea.containsMouse ? 0.6 : 0
            Behavior on opacity { NumberAnimation { duration: 150 } }
        }

        MouseArea {
            id: portMouseArea
            anchors.fill: parent
            anchors.margins: -4
            hoverEnabled: true
            cursorShape: Qt.CrossCursor

            property bool isDragging: false

            onPressed: (mouse) => {
                isDragging = true
                patcherRoot.startCableDrag(port, isInlet, portCircle, portColor)
                mouse.accepted = true
            }
            onPositionChanged: (mouse) => {
                if (isDragging) {
                    // Map from portMouseArea coords to root coords
                    var pos = portMouseArea.mapToItem(patcherRoot, mouse.x, mouse.y)
                    patcherRoot.updateCableDrag(Qt.point(pos.x, pos.y))
                }
            }
            onReleased: (mouse) => {
                if (isDragging) {
                    var pos = portMouseArea.mapToItem(patcherRoot, mouse.x, mouse.y)
                    patcherRoot.finishCableDrag(Qt.point(pos.x, pos.y))
                    isDragging = false
                }
            }
        }
    }

    // Port label
    Text {
        text: portName
        color: "#cccccc"
        font.pixelSize: 10
        anchors.verticalCenter: parent.verticalCenter
        x: isInlet ? 10 : 4
        width: nodeWidth - 20
        elide: Text.ElideRight
        horizontalAlignment: isInlet ? Text.AlignLeft : Text.AlignRight
    }

    // Make port globally findable for cable routing
    function getGlobalCenter() {
        return portCircle.mapToItem(null, portCircle.width / 2, portCircle.height / 2)
    }
}
