import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// The markup tools, down the side of the sheet.
Rectangle {
    id: tools
    property string tool: "pan"
    signal picked(string tool)
    signal scaleRequested()

    width: column.implicitWidth + 8
    height: column.implicitHeight + 8
    radius: Math.min(6, theme.radius)
    color: "#131316"
    border.color: "#2a2a30"

    readonly property var entries: [
        { tool: "pan", key: "V", text: qsTr("Pan") },
        { tool: "cloud", key: "R", text: qsTr("Cloud") },
        { tool: "arrow", key: "A", text: qsTr("Arrow") },
        { tool: "text", key: "T", text: qsTr("Text") },
        { tool: "measure", key: "M", text: qsTr("Measure") },
        { tool: "scale", key: "S", text: qsTr("Scale") }
    ]

    Column {
        id: column
        anchors.centerIn: parent
        spacing: 2

        Repeater {
            model: tools.entries
            delegate: Button {
                id: button
                required property var modelData
                readonly property bool active: tools.tool === modelData.tool
                    || (modelData.tool === "scale" && tools.tool === "calibrate")
                width: Math.max(implicitWidth, column.width)
                flat: true
                leftPadding: 10
                rightPadding: 10
                topInset: 0
                bottomInset: 0
                implicitHeight: 32
                Material.foreground: active ? theme.accentForeground : "#d8d8de"
                background: Rectangle {
                    radius: Math.min(4, theme.radius)
                    color: button.active ? theme.accent : button.hovered ? "#24242a" : "transparent"
                }
                contentItem: Row {
                    spacing: 10
                    Label {
                        text: button.modelData.key
                        font.family: "monospace"
                        font.pixelSize: 12
                        color: button.active ? theme.accentForeground : theme.accent
                        width: 10
                    }
                    Label {
                        text: button.modelData.text
                        font.pixelSize: 13
                        color: button.active ? theme.accentForeground : "#d8d8de"
                    }
                }
                onClicked: modelData.tool === "scale" ? tools.scaleRequested() : tools.picked(modelData.tool)
            }
        }
    }
}
