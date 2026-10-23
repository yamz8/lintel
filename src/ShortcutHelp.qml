import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Every shortcut, shown with ?, in two groups side by side.
Rectangle {
    id: help
    color: "#cc0b0b0d"

    readonly property var groups: [
        {
            title: qsTr("View and compare"),
            shortcuts: [
                ["O", qsTr("Open a drawing set")],
                ["C", qsTr("Compare with an older revision")],
                ["X", qsTr("Close the older revision")],
                ["Space", qsTr("Toggle the overlay")],
                ["1 / 2 / 3", qsTr("Current, previous, overlay")],
                ["] / [", qsTr("Next and previous change")],
                ["N / Shift+N", qsTr("Next and previous changed sheet")],
                ["J / Page Down", qsTr("Next sheet")],
                ["K / Page Up", qsTr("Previous sheet")],
                ["Home / End", qsTr("First and last sheet")],
                ["+ / −", qsTr("Zoom in and out")],
                ["0", qsTr("Fit the sheet")],
                [qsTr("Wheel"), qsTr("Zoom at the pointer")],
                [qsTr("Drag"), qsTr("Pan")]
            ]
        },
        {
            title: qsTr("Mark up"),
            shortcuts: [
                ["V", qsTr("Pan, and pick markups")],
                ["R / A / T", qsTr("Cloud, arrow, text")],
                ["M", qsTr("Measure")],
                ["S", qsTr("Set the sheet's scale")],
                ["Delete", qsTr("Delete the picked markup")],
                ["Ctrl+Z", qsTr("Undo")],
                ["Ctrl+Shift+Z", qsTr("Redo")],
                ["E", qsTr("Export marked-up sheets to PDF")],
                ["Esc", qsTr("Back to panning")],
                ["?", qsTr("Show or hide shortcuts")],
                ["Q", qsTr("Quit")]
            ]
        }
    ]

    TapHandler { onTapped: help.visible = false }

    Pane {
        anchors.centerIn: parent
        padding: 24
        Material.roundedScale: theme.radius
        background: Rectangle { color: "#17171a"; radius: theme.radius; border.color: "#2a2a30" }

        ColumnLayout {
            spacing: 14

            Label {
                text: qsTr("Keyboard shortcuts")
                font.pixelSize: 18
                font.bold: true
            }

            RowLayout {
                spacing: 40

                Repeater {
                    model: help.groups
                    delegate: ColumnLayout {
                        required property var modelData
                        Layout.alignment: Qt.AlignTop
                        spacing: 8

                        Label {
                            text: parent.modelData.title
                            color: "#8a8a94"
                            font.pixelSize: 12
                            font.capitalization: Font.AllUppercase
                        }

                        GridLayout {
                            columns: 2
                            columnSpacing: 20
                            rowSpacing: 7

                            Repeater {
                                model: parent.parent.modelData.shortcuts
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
                                model: parent.parent.modelData.shortcuts
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
        }
    }
}
