import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Sets the scale measurements read in: a standard drawing scale, or one
// worked out from a line drawn over a dimension the user knows.
Dialog {
    id: dialog
    property int sheet: 0
    // How long the calibration line is in points; 0 when picking a preset.
    property real calibrationLength: 0
    signal calibrateRequested()

    readonly property bool calibrating: calibrationLength > 0
    readonly property var presets: markupSet.presets()

    title: calibrating ? qsTr("How long is that line?") : qsTr("Sheet scale")
    modal: true
    anchors.centerIn: parent
    width: Math.min(parent.width - 40, 420)
    Material.roundedScale: theme.radius
    standardButtons: Dialog.Ok | Dialog.Cancel
    Material.background: "#17171a"
    background: Rectangle {
        color: "#17171a"
        radius: Math.min(8, theme.radius)
        border.color: "#2a2a30"
    }
    Overlay.modal: Rectangle { color: "#99000000" }

    function pick(length) {
        calibrationLength = length
        problem.text = ""
        lengthField.text = ""
        open()
    }

    onOpened: if (calibrating) lengthField.forceActiveFocus()

    onAccepted: {
        if (calibrating) {
            const unit = units.currentIndex === 0 ? "ft" : "m"
            const length = markupSet.parseLength(lengthField.text, unit)
            if (length <= 0) {
                problem.text = unit === "ft" ? qsTr("Type a length like 12'-6\" or 12.5.") : qsTr("Type a length like 3.5.")
                Qt.callLater(dialog.open)
                return
            }
            markupSet.calibrate(sheet, calibrationLength, length, unit, everySheet.checked)
        } else {
            const preset = presets[presetBox.currentIndex]
            markupSet.setScale(sheet, preset.pointsPerUnit, preset.unit, preset.label, everySheet.checked)
        }
        calibrationLength = 0
    }
    onRejected: calibrationLength = 0

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        Label {
            visible: !dialog.calibrating
            text: markupSet.revision >= 0 && markupSet.scaleLabel(dialog.sheet)
                ? qsTr("This sheet is at %1.").arg(markupSet.scaleLabel(dialog.sheet))
                : qsTr("This sheet has no scale yet.")
            color: "#b8b8c0"
        }

        ComboBox {
            id: presetBox
            visible: !dialog.calibrating
            Layout.fillWidth: true
            model: dialog.presets
            textRole: "label"
            currentIndex: 2
        }

        Button {
            visible: !dialog.calibrating
            flat: true
            text: qsTr("Calibrate from a known dimension…")
            onClicked: {
                dialog.calibrationLength = 0
                dialog.close()
                dialog.calibrateRequested()
            }
        }

        RowLayout {
            visible: dialog.calibrating
            Layout.fillWidth: true
            spacing: 8

            TextField {
                id: lengthField
                Layout.fillWidth: true
                placeholderText: units.currentIndex === 0 ? "12'-6\"" : "3.5"
                onAccepted: dialog.accept()
            }
            ComboBox {
                id: units
                model: [qsTr("Feet"), qsTr("Meters")]
            }
        }

        Label {
            id: problem
            visible: text !== ""
            color: "#f08a84"
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }

        CheckBox {
            id: everySheet
            text: qsTr("Use for every sheet")
            checked: true
        }
    }
}
