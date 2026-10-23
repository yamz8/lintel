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
    Shortcut {
        sequence: "Escape"
        context: Qt.ApplicationShortcut
        onActivated: win.helpVisible ? win.helpVisible = false : backend.clearError()
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
                    }

                    // The zoom level, and the overlay's key while it's on.
                    Rectangle {
                        visible: win.hasSet
                        anchors.left: parent.left
                        anchors.bottom: parent.bottom
                        anchors.margins: 12
                        width: chip.implicitWidth + 20
                        height: chip.implicitHeight + 12
                        radius: Math.min(4, theme.radius)
                        color: "#e6131316"
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

        ShortcutHelp {
            anchors.fill: parent
            visible: win.helpVisible
            onVisibleChanged: win.helpVisible = visible
        }
    }
}
