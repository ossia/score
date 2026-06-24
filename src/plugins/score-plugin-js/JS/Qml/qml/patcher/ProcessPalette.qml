import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: processPalette

    property var editContext: null
    property var patcherRoot: null
    property var nodeContainer: null
    property real zoomLevel: 1.0
    property point panOffset: Qt.point(0, 0)

    width: 300
    height: 400
    modal: false
    dim: false

    property var processList: editContext ? editContext.availableProcesses() : ({})

    background: Rectangle {
        color: "#2a2a4e"
        border.color: "#4a4a6e"
        border.width: 1
        radius: 6
    }

    contentItem: ColumnLayout {
        spacing: 4

        // Search field
        TextField {
            id: searchField
            Layout.fillWidth: true
            placeholderText: "Search processes..."
            color: "white"
            font.pixelSize: 12
            background: Rectangle {
                color: "#1a1a3e"
                radius: 4
                border.color: "#4a4a6e"
            }
        }

        // Process list
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: filterProcesses()

            delegate: ItemDelegate {
                width: ListView.view.width
                height: 32

                background: Rectangle {
                    color: hovered ? "#3a3a5e" : "transparent"
                    radius: 3
                }

                contentItem: RowLayout {
                    spacing: 8

                    // Category color dot
                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: patcherRoot.categoryColor(modelData.category || "")
                    }

                    Text {
                        text: modelData.name || modelData.key || ""
                        color: "#cccccc"
                        font.pixelSize: 11
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }

                    Text {
                        text: modelData.category || ""
                        color: "#888888"
                        font.pixelSize: 9
                    }
                }

                onClicked: {
                    createProcessAtPalette(modelData.key)
                    processPalette.close()
                }
            }

            ScrollBar.vertical: ScrollBar { }
        }
    }

    function filterProcesses() {
        var result = []
        var filter = searchField.text.toLowerCase()
        for (var key in processList) {
            var info = processList[key]
            var name = info.Name || ""
            var category = info.Category || ""
            if (filter === "" || name.toLowerCase().includes(filter)
                || category.toLowerCase().includes(filter)) {
                result.push({
                    key: key,
                    name: name,
                    category: category,
                    description: info.Description || ""
                })
            }
        }
        // Sort by name
        result.sort(function(a, b) {
            return a.name.localeCompare(b.name)
        })
        return result
    }

    function createProcessAtPalette(processKey) {
        if (!editContext || !patcherRoot) return
        var container = patcher ? patcher.container : null
        if (!container) return

        // Create at palette position
        var pos = mapToItem(nodeContainer, processPalette.x, processPalette.y)
        var proc = editContext.createProcess(container, processKey, "")
        if (proc) {
            editContext.moveNode(proc, Qt.point(
                pos.x / zoomLevel,
                pos.y / zoomLevel))
        }
    }
}
