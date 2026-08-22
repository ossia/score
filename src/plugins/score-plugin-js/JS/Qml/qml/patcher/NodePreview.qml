import QtQuick

Item {
    id: previewItem
    property var node: null

    width: parent ? parent.width : 160
    height: width * 9 / 16

    Rectangle {
        anchors.fill: parent
        anchors.margins: 2
        color: "#111122"
        radius: 2

        // Placeholder text when no texture available
        Text {
            anchors.centerIn: parent
            text: "Preview"
            color: "#555577"
            font.pixelSize: 10
            visible: true  // TODO: hide when TextureSource is active
        }

        // TODO: Use TextureSource when score is executing:
        // TextureSource {
        //     anchors.fill: parent
        //     process: node ? node.objectName : ""
        //     outlet: 0
        // }
    }
}
