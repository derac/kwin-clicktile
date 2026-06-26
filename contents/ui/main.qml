import QtQuick

Item {
    id: root

    property bool overlayVisible: false
    property int columns: 2
    property int rows: 2
    property int selectionLeft: 0
    property int selectionTop: 0
    property int selectionRight: 0
    property int selectionBottom: 0
    property color gridColor: "#be50c8ff"
    property color selectionColor: "#5f50a0ff"
    property color selectionBorderColor: "#e6ffffff"
    property real workAreaX: 0
    property real workAreaY: 0
    property real workAreaWidth: width
    property real workAreaHeight: height

    visible: overlayVisible
    opacity: overlayVisible ? 0.38 : 0

    readonly property bool hasSelection: selectionRight > selectionLeft && selectionBottom > selectionTop

    Item {
        id: gridLayer

        x: root.workAreaX
        y: root.workAreaY
        width: root.workAreaWidth
        height: root.workAreaHeight

        readonly property real cellWidth: root.columns > 0 ? width / root.columns : width
        readonly property real cellHeight: root.rows > 0 ? height / root.rows : height

        Rectangle {
            anchors.fill: parent
            color: "transparent"
            border.width: 1
            border.color: root.gridColor
        }

        Rectangle {
            visible: root.hasSelection
            x: root.selectionLeft * gridLayer.cellWidth
            y: root.selectionTop * gridLayer.cellHeight
            width: (root.selectionRight - root.selectionLeft) * gridLayer.cellWidth
            height: (root.selectionBottom - root.selectionTop) * gridLayer.cellHeight
            color: root.selectionColor
            border.width: 2
            border.color: root.selectionBorderColor
        }

        Repeater {
            model: Math.max(0, root.columns - 1)

            Rectangle {
                x: Math.round((index + 1) * gridLayer.cellWidth)
                width: 1
                height: gridLayer.height
                color: root.gridColor
            }
        }

        Repeater {
            model: Math.max(0, root.rows - 1)

            Rectangle {
                y: Math.round((index + 1) * gridLayer.cellHeight)
                width: gridLayer.width
                height: 1
                color: root.gridColor
            }
        }
    }
}
