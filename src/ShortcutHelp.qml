import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Every shortcut, shown with ?.
Rectangle {
    id: help
    color: "#cc0b0b0d"

    readonly property var shortcuts: [
        ["O", qsTr("Open a drawing set")],
        ["C", qsTr("Compare with an older revision")],
        ["X", qsTr("Close the older revision")],
        ["Space", qsTr("Toggle the overlay")],
        ["1 / 2 / 3", qsTr("Current, previous, overlay")],
        ["J / Page Down", qsTr("Next sheet")],
        ["K / Page Up", qsTr("Previous sheet")],
        ["Home / End", qsTr("First and last sheet")],
        ["+ / −", qsTr("Zoom in and out")],
        ["0", qsTr("Fit the sheet")],
        ["← ↑ → ↓", qsTr("Pan")],
        [qsTr("Wheel"), qsTr("Zoom at the pointer")],
        [qsTr("Drag"), qsTr("Pan")],
        ["?", qsTr("Show or hide shortcuts")],
        ["Q", qsTr("Quit")]
    ]

    TapHandler { onTapped: help.visible = false }

    Pane {
        anchors.centerIn: parent
        padding: 24
        Material.elevation: 8
        Material.roundedScale: theme.radius
        background: Rectangle { color: "#17171a"; radius: theme.radius; border.color: "#2a2a30" }

        ColumnLayout {
            spacing: 14

            Label {
                text: qsTr("Keyboard shortcuts")
                font.pixelSize: 18
                font.bold: true
            }

            GridLayout {
                columns: 2
                columnSpacing: 24
                rowSpacing: 8

                Repeater {
                    model: help.shortcuts
                    delegate: Label {
                        required property var modelData
                        required property int index
                        Layout.row: index
                        Layout.column: 0
                        text: modelData[0]
                        color: theme.accent
                        font.family: "monospace"
                    }
                }
                Repeater {
                    model: help.shortcuts
                    delegate: Label {
                        required property var modelData
                        required property int index
                        Layout.row: index
                        Layout.column: 1
                        text: modelData[1]
                        color: "#d8d8de"
                    }
                }
            }
        }
    }
}
