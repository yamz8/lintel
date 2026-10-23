import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1280
    height: 820
    minimumWidth: 720
    minimumHeight: 480
    visible: true
    title: backend.fileName ? "Lintel — " + backend.fileName : "Lintel"
    color: "#0e0e10"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    property bool helpVisible: false
    readonly property bool hasSet: backend.sheetCount > 0
    readonly property bool sheetMissing: backend.hasPrevious && backend.previousSheet < 0
    property string lastSingleMode: "current"

    function showMode(mode) {
        if (mode !== "overlay")
            lastSingleMode = mode
        backend.mode = mode
    }

    // Reads the language so bindings that format numbers re-render when
    // Omarchy's language changes.
    function number(value) {
        return translator.language !== "" ? value.toLocaleString(Qt.locale(), 'f', 0) : ""
    }

    // Which change on the sheet is being looked at; -1 for none.
    property int region: -1
    property int pendingRegion: -2
    readonly property var regions: backend.analysisRevision >= 0 && backend.hasPrevious
        ? backend.changeRegions(backend.sheet) : []

    Connections {
        target: backend
        function onSheetChanged() {
            win.region = -1
            viewer.selectedMarkup = -1
            // Runs after the viewer fits the new sheet.
            if (win.pendingRegion !== -2)
                Qt.callLater(win.showPendingRegion)
        }
    }

    function showPendingRegion() {
        const wanted = pendingRegion
        pendingRegion = -2
        if (regions.length === 0)
            return
        region = wanted < 0 ? regions.length - 1 : Math.min(wanted, regions.length - 1)
        viewer.showRegion(regions[region])
    }

    // Steps through every change in the set: the areas on this sheet first,
    // then on to the next changed or new sheet.
    function stepChange(step) {
        if (!backend.hasPrevious)
            return
        const next = region + step
        if (next >= 0 && next < regions.length) {
            region = next
            viewer.showRegion(regions[next])
            return
        }
        stepSheet(step)
    }

    function stepSheet(step) {
        const sheet = backend.nextChangedSheet(backend.sheet, step)
        if (sheet < 0 || sheet === backend.sheet)
            return
        pendingRegion = step > 0 ? 0 : -1
        backend.sheet = sheet
    }

    // The markup tool in use; "pan" looks around and picks markups.
    property string tool: "pan"

    function pickTool(next) {
        tool = next
        viewer.selectedMarkup = -1
    }

    function deleteMarkup() {
        if (viewer.selectedMarkup >= 0) {
            markupSet.remove(backend.sheet, viewer.selectedMarkup)
            viewer.selectedMarkup = -1
        }
    }

    function toggleOverlay() {
        if (backend.hasPrevious)
            showMode(backend.mode === "overlay" ? lastSingleMode : "overlay")
    }

    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; onActivated: Qt.quit() }
    Shortcut { sequences: ["O", StandardKey.Open]; context: Qt.ApplicationShortcut; onActivated: backend.openDrawingSet() }
    Shortcut { sequence: "C"; context: Qt.ApplicationShortcut; onActivated: backend.openPrevious() }
    Shortcut { sequence: "X"; context: Qt.ApplicationShortcut; enabled: backend.hasPrevious; onActivated: backend.closePrevious() }
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; onActivated: win.toggleOverlay() }
    Shortcut { sequence: "1"; context: Qt.ApplicationShortcut; onActivated: win.showMode("current") }
    Shortcut { sequence: "2"; context: Qt.ApplicationShortcut; enabled: backend.hasPrevious; onActivated: win.showMode("previous") }
    Shortcut { sequence: "3"; context: Qt.ApplicationShortcut; enabled: backend.hasPrevious; onActivated: win.showMode("overlay") }
    Shortcut { sequence: "N"; context: Qt.ApplicationShortcut; onActivated: win.stepSheet(1) }
    Shortcut { sequence: "Shift+N"; context: Qt.ApplicationShortcut; onActivated: win.stepSheet(-1) }
    Shortcut { sequence: "]"; context: Qt.ApplicationShortcut; onActivated: win.stepChange(1) }
    Shortcut { sequence: "["; context: Qt.ApplicationShortcut; onActivated: win.stepChange(-1) }
    Shortcut { sequences: ["J", "PgDown"]; context: Qt.ApplicationShortcut; onActivated: backend.nextSheet() }
    Shortcut { sequences: ["K", "PgUp"]; context: Qt.ApplicationShortcut; onActivated: backend.previousSheetInSet() }
    Shortcut { sequence: "Home"; context: Qt.ApplicationShortcut; onActivated: backend.sheet = 0 }
    Shortcut { sequence: "End"; context: Qt.ApplicationShortcut; onActivated: backend.sheet = backend.sheetCount - 1 }
    Shortcut { sequences: ["+", "=", StandardKey.ZoomIn]; context: Qt.ApplicationShortcut; onActivated: viewer.zoomBy(1.25) }
    Shortcut { sequences: ["-", StandardKey.ZoomOut]; context: Qt.ApplicationShortcut; onActivated: viewer.zoomBy(0.8) }
    Shortcut { sequences: ["0", "F"]; context: Qt.ApplicationShortcut; onActivated: viewer.fit() }
    Shortcut { sequence: "Left"; context: Qt.ApplicationShortcut; onActivated: viewer.pan(-80, 0) }
    Shortcut { sequence: "Right"; context: Qt.ApplicationShortcut; onActivated: viewer.pan(80, 0) }
    Shortcut { sequence: "Up"; context: Qt.ApplicationShortcut; onActivated: viewer.pan(0, -80) }
    Shortcut { sequence: "Down"; context: Qt.ApplicationShortcut; onActivated: viewer.pan(0, 80) }
    Shortcut { sequence: "V"; context: Qt.ApplicationShortcut; onActivated: win.pickTool("pan") }
    Shortcut { sequence: "R"; context: Qt.ApplicationShortcut; enabled: win.hasSet; onActivated: win.pickTool("cloud") }
    Shortcut { sequence: "A"; context: Qt.ApplicationShortcut; enabled: win.hasSet; onActivated: win.pickTool("arrow") }
    Shortcut { sequence: "T"; context: Qt.ApplicationShortcut; enabled: win.hasSet; onActivated: win.pickTool("text") }
    Shortcut { sequence: "M"; context: Qt.ApplicationShortcut; enabled: win.hasSet; onActivated: win.pickTool("measure") }
    Shortcut { sequence: "S"; context: Qt.ApplicationShortcut; enabled: win.hasSet; onActivated: scaleDialog.pick(0) }
    Shortcut { sequences: [StandardKey.Delete, "Backspace"]; context: Qt.ApplicationShortcut; onActivated: win.deleteMarkup() }
    Shortcut { sequences: [StandardKey.Undo]; context: Qt.ApplicationShortcut; onActivated: { viewer.selectedMarkup = -1; markupSet.undo() } }
    Shortcut { sequences: [StandardKey.Redo, "Ctrl+Shift+Z"]; context: Qt.ApplicationShortcut; onActivated: { viewer.selectedMarkup = -1; markupSet.redo() } }
    // Escape steps back one thing at a time: help, then the tool or the
    // picked markup, then the error.
    Shortcut {
        sequence: "Escape"
        context: Qt.ApplicationShortcut
        // Open dialogs close themselves on Escape.
        enabled: !scaleDialog.visible && !viewer.promptOpen
        onActivated: {
            if (win.helpVisible)
                win.helpVisible = false
            else if (win.tool !== "pan" || viewer.selectedMarkup >= 0)
                win.pickTool("pan")
            else
                backend.clearError()
        }
    }

    // Right-to-left languages mirror the window; the Translator sets the
    // application's layout direction.
    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // The bar: what's open, which sheet, and how it's shown.
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 52
                color: "#131316"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 8
                    spacing: 12

                    Label {
                        text: "Lintel"
                        font.pixelSize: 16
                        font.bold: true
                        color: theme.accent
                    }

                    ColumnLayout {
                        visible: win.hasSet
                        spacing: 0
                        Layout.fillWidth: true
                        Layout.minimumWidth: 60
                        Layout.maximumWidth: 420

                        Label {
                            Layout.fillWidth: true
                            text: backend.fileName
                            elide: Text.ElideMiddle
                            font.pixelSize: 13
                        }
                        RowLayout {
                            visible: backend.hasPrevious
                            Layout.fillWidth: true
                            spacing: 4

                            Label {
                                // Natural width when there's room, elided when not,
                                // so the close mark stays beside the name.
                                Layout.fillWidth: true
                                Layout.maximumWidth: implicitWidth
                                text: qsTr("compared with %1").arg(backend.previousFileName)
                                elide: Text.ElideMiddle
                                font.pixelSize: 11
                                color: "#8a8a94"
                            }
                            // Stops comparing; small, beside the file it closes.
                            Label {
                                text: "✕"
                                font.pixelSize: 11
                                color: closeHover.hovered ? theme.accent : "#8a8a94"
                                HoverHandler { id: closeHover; cursorShape: Qt.PointingHandCursor }
                                TapHandler { onTapped: backend.closePrevious() }
                                ToolTip.visible: closeHover.hovered
                                ToolTip.text: qsTr("Stop comparing (X)")
                            }
                        }
                    }

                    Label {
                        visible: win.hasSet && win.width >= 1000
                        text: qsTr("Sheet %1 · %2 of %3").arg(backend.sheetLabel(backend.sheet))
                            .arg(win.number(backend.sheet + 1))
                            .arg(win.number(backend.sheetCount))
                        color: "#b8b8c0"
                        font.pixelSize: 13
                    }

                    // How the comparison came out, or how far along it is.
                    Label {
                        visible: backend.hasPrevious && win.width >= 1100
                        text: backend.scanning
                            ? qsTr("Comparing… %1%").arg(win.number(Math.round(backend.scanProgress * 100)))
                            : [qsTr("%n changed", "", backend.changedCount),
                               qsTr("%n new", "", backend.newCount),
                               backend.removedCount > 0 ? qsTr("%n removed", "", backend.removedCount) : ""]
                                  .filter(part => part !== "").join(" · ")
                        color: "#D98E04"
                        font.pixelSize: 13
                    }

                    Item { Layout.fillWidth: true }

                    // Which revision to show, once there are two.
                    Row {
                        visible: backend.hasPrevious
                        spacing: 0

                        Repeater {
                            model: [
                                { mode: "current", text: qsTr("Current") },
                                { mode: "previous", text: qsTr("Previous") },
                                { mode: "overlay", text: qsTr("Overlay") }
                            ]
                            delegate: Button {
                                required property var modelData
                                text: modelData.text
                                readonly property bool active: backend.mode === modelData.mode
                                Material.foreground: active ? theme.accentForeground : "#d8d8de"
                                background: Rectangle {
                                    implicitHeight: 32
                                    radius: Math.min(4, theme.radius)
                                    color: parent.active ? theme.accent : parent.hovered ? "#24242a" : "transparent"
                                }
                                onClicked: win.showMode(modelData.mode)
                            }
                        }
                    }

                    ToolButton {
                        text: qsTr("Open")
                        onClicked: backend.openDrawingSet()
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Open a drawing set (O)")
                    }
                    ToolButton {
                        visible: win.hasSet && !backend.hasPrevious
                        text: qsTr("Compare…")
                        onClicked: backend.openPrevious()
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Compare with an older revision (C)")
                    }
                    ToolButton {
                        text: "?"
                        onClicked: win.helpVisible = !win.helpVisible
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Keyboard shortcuts (?)")
                    }
                }

                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#222228" }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                SheetStrip {
                    visible: win.hasSet
                    Layout.preferredWidth: 200
                    Layout.fillHeight: true
                }

                Rectangle {
                    visible: win.hasSet
                    Layout.preferredWidth: 1
                    Layout.fillHeight: true
                    color: "#222228"
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    SheetViewer {
                        id: viewer
                        anchors.fill: parent
                        visible: win.hasSet
                        kind: backend.mode === "previous" && win.sheetMissing ? "" : backend.mode
                        regions: win.regions
                        focusedRegion: win.region
                        tool: win.tool
                        onCalibrated: (length) => {
                            win.pickTool("pan")
                            scaleDialog.pick(length)
                        }
                    }

                    MarkupTools {
                        visible: win.hasSet
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.margins: 12
                        tool: win.tool
                        onPicked: (next) => win.pickTool(next)
                        onScaleRequested: scaleDialog.pick(0)
                    }

                    // What the tool in use does.
                    Rectangle {
                        visible: win.hasSet && win.tool !== "pan"
                        anchors.bottom: parent.bottom
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottomMargin: 12
                        width: hint.implicitWidth + 24
                        height: hint.implicitHeight + 12
                        radius: Math.min(4, theme.radius)
                        color: "#131316"
                        border.color: theme.accent

                        Label {
                            id: hint
                            anchors.centerIn: parent
                            font.pixelSize: 12
                            text: {
                                switch (win.tool) {
                                case "cloud": return qsTr("Drag to draw a revision cloud. Esc to stop.")
                                case "arrow": return qsTr("Drag from the tail to the tip. Esc to stop.")
                                case "text": return qsTr("Click where the note goes. Esc to stop.")
                                case "measure": return qsTr("Drag to measure. Esc to stop.")
                                case "calibrate": return qsTr("Drag along a dimension you know the length of.")
                                }
                                return ""
                            }
                        }                    }

                    // The zoom level, and the overlay's key while it's on.
                    Rectangle {
                        visible: win.hasSet
                        anchors.left: parent.left
                        anchors.bottom: parent.bottom
                        anchors.margins: 12
                        width: chip.implicitWidth + 20
                        height: chip.implicitHeight + 12
                        radius: Math.min(4, theme.radius)
                        color: "#131316"
                        border.color: "#2a2a30"

                        Row {
                            id: chip
                            anchors.centerIn: parent
                            spacing: 12

                            Label {
                                text: qsTr("%1%").arg(win.number(Math.round(viewer.zoom * 100 / viewer.fitZoom)))
                                color: "#b8b8c0"
                                font.pixelSize: 12
                            }

                            Label {
                                readonly property string scaleLabel: markupSet.revision >= 0 ? markupSet.scaleLabel(backend.sheet) : ""
                                visible: scaleLabel !== "" || win.tool === "measure"
                                text: scaleLabel !== "" ? qsTr("Scale %1").arg(scaleLabel) : qsTr("No scale: press S")
                                color: scaleLabel !== "" ? "#b8b8c0" : "#f0b45a"
                                font.pixelSize: 12
                            }

                            Label {
                                visible: backend.analysisRevision >= 0 && backend.isAligned(backend.sheet)
                                text: qsTr("Aligned")
                                color: "#8a8a94"
                                font.pixelSize: 12
                                ToolTip.visible: alignedHover.hovered
                                ToolTip.text: qsTr("The previous revision was shifted or scaled to line up with this one.")
                                HoverHandler { id: alignedHover }
                            }

                            Label {
                                visible: win.regions.length > 0
                                text: win.region >= 0
                                    ? qsTr("Change %1 of %2").arg(win.number(win.region + 1)).arg(win.number(win.regions.length))
                                    : qsTr("%n changes", "", win.regions.length)
                                color: "#D98E04"
                                font.pixelSize: 12
                            }

                            Repeater {
                                model: backend.mode === "overlay" ? [
                                    { color: "#E0302A", text: qsTr("Removed") },
                                    { color: "#1FA34A", text: qsTr("Added") }
                                ] : []
                                delegate: Row {
                                    required property var modelData
                                    spacing: 5
                                    Rectangle {
                                        width: 10; height: 10
                                        radius: Math.min(2, theme.radius)
                                        color: modelData.color
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                    Label { text: modelData.text; font.pixelSize: 12; color: "#d8d8de" }
                                }
                            }
                        }
                    }

                    // A sheet the older revision doesn't have: say so rather
                    // than show nothing, or everything in green unexplained.
                    Rectangle {
                        visible: win.hasSet && win.sheetMissing && backend.mode !== "current"
                        anchors.top: parent.top
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.topMargin: 12
                        width: missingLabel.implicitWidth + 24
                        height: missingLabel.implicitHeight + 12
                        radius: Math.min(4, theme.radius)
                        color: "#1d2a20"
                        border.color: "#1FA34A"

                        Label {
                            id: missingLabel
                            anchors.centerIn: parent
                            text: qsTr("This sheet isn't in %1, so all of it is new.").arg(backend.previousFileName)
                            color: "#c8ecd2"
                            font.pixelSize: 13
                        }
                    }

                    ColumnLayout {
                        visible: !win.hasSet
                        anchors.centerIn: parent
                        spacing: 14

                        Label {
                            Layout.alignment: Qt.AlignHCenter
                            text: qsTr("Open a drawing set")
                            font.pixelSize: 22
                            font.bold: true
                        }
                        Label {
                            Layout.alignment: Qt.AlignHCenter
                            text: qsTr("Press O, or drop a PDF here.")
                            color: "#8a8a94"
                        }
                        Button {
                            Layout.alignment: Qt.AlignHCenter
                            text: qsTr("Open drawing set")
                            leftPadding: 20
                            rightPadding: 20
                            Material.foreground: theme.accentForeground
                            background: Rectangle {
                                implicitHeight: 40
                                radius: Math.min(6, theme.radius)
                                color: parent.down ? Qt.darker(theme.accent, 1.15) : theme.accent
                            }
                            onClicked: backend.openDrawingSet()
                        }
                    }
                }
            }
        }

        // What went wrong, until it's dismissed or the next thing works.
        Rectangle {
            visible: backend.error !== ""
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottomMargin: 20
            width: Math.min(parent.width - 40, errorRow.implicitWidth + 24)
            height: errorRow.implicitHeight + 16
            radius: Math.min(6, theme.radius)
            color: "#2a1414"
            border.color: "#E0302A"

            RowLayout {
                id: errorRow
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 6
                spacing: 8

                Label {
                    Layout.fillWidth: true
                    text: backend.error
                    color: "#f4d0ce"
                    wrapMode: Text.Wrap
                }
                ToolButton {
                    text: "✕"
                    onClicked: backend.clearError()
                }
            }
        }

        // Dropping PDFs: on an empty window it opens the set; with a set
        // open, the drop picks which side the file goes to.
        DropArea {
            id: drop
            anchors.fill: parent
            keys: ["text/uri-list"]
            onDropped: (event) => {
                if (event.hasUrls && !win.hasSet)
                    backend.load(event.urls[0])
            }
        }

        Rectangle {
            anchors.fill: parent
            // Always there while a set is open, so its drop areas catch the
            // drag from the moment it enters; it only shows during one.
            visible: win.hasSet
            opacity: dropCurrent.containsDrag || dropPrevious.containsDrag ? 1 : 0
            color: "#dd0b0b0d"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 24
                spacing: 24

                Repeater {
                    model: 2
                    delegate: Rectangle {
                        required property int index
                        readonly property DropArea area: index === 0 ? dropCurrent : dropPrevious
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: theme.radius
                        color: area.containsDrag ? Qt.alpha(theme.accent, 0.18) : "transparent"
                        border.color: area.containsDrag ? theme.accent : "#3a3a42"
                        border.width: 2

                        Label {
                            anchors.centerIn: parent
                            text: index === 0 ? qsTr("Open as the drawing set") : qsTr("Compare with this revision")
                            font.pixelSize: 18
                            color: parent.area.containsDrag ? theme.accent : "#b8b8c0"
                        }
                    }
                }
            }

            RowLayout {
                anchors.fill: parent
                spacing: 0

                DropArea {
                    id: dropCurrent
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    keys: ["text/uri-list"]
                    onDropped: (event) => backend.load(event.urls[0])
                }
                DropArea {
                    id: dropPrevious
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    keys: ["text/uri-list"]
                    onDropped: (event) => backend.loadPrevious(event.urls[0])
                }
            }
        }

        ScaleDialog {
            id: scaleDialog
            sheet: backend.sheet
            onCalibrateRequested: win.pickTool("calibrate")
        }

        ShortcutHelp {
            anchors.fill: parent
            visible: win.helpVisible
            onVisibleChanged: win.helpVisible = visible
        }
    }
}
