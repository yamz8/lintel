import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import Lintel

// One sheet on paper, zoomed and panned. The image on screen scales at once
// and the sheet re-renders at the new resolution once the zoom settles, so
// zooming stays smooth on large sheets.
Item {
    id: viewer

    property string kind: "current"
    readonly property size points: backend.sheetSize
    readonly property real margin: 24
    readonly property real fitZoom: points.width > 0
        ? Math.max(0.01, Math.min((width - 2 * margin) / points.width, (height - 2 * margin) / points.height))
        : 1
    readonly property real minZoom: fitZoom / 2
    readonly property real maxZoom: Math.max(fitZoom * 2, 12)
    // The base image never renders bigger than this; past it, sharp tiles
    // of just the visible part are drawn over it.
    readonly property int baseEdge: 3072
    readonly property int tileEdge: 512
    readonly property size fullSize: Qt.size(Math.round(points.width * renderZoom * Screen.devicePixelRatio),
                                             Math.round(points.height * renderZoom * Screen.devicePixelRatio))
    readonly property bool tiled: Math.max(fullSize.width, fullSize.height) > baseEdge
    property var tiles: []

    function updateTiles() {
        if (!tiled || kind === "" || paper.width <= 0) {
            tiles = []
            return
        }
        // The visible part of the sheet in full-size pixels, plus a tile of
        // margin so panning a little shows sharp tiles at once.
        const scale = fullSize.width / paper.width
        const left = Math.max(0, (flick.contentX - paper.x) * scale - tileEdge)
        const top = Math.max(0, (flick.contentY - paper.y) * scale - tileEdge)
        const right = Math.min(fullSize.width, (flick.contentX - paper.x + flick.width) * scale + tileEdge)
        const bottom = Math.min(fullSize.height, (flick.contentY - paper.y + flick.height) * scale + tileEdge)
        const list = []
        for (let y = Math.floor(top / tileEdge) * tileEdge; y < bottom; y += tileEdge)
            for (let x = Math.floor(left / tileEdge) * tileEdge; x < right; x += tileEdge)
                list.push({ x: x, y: y, w: Math.min(tileEdge, fullSize.width - x), h: Math.min(tileEdge, fullSize.height - y) })
        tiles = list
    }
    // Logical pixels per PDF point.
    property real zoom: 1
    property real renderZoom: 1
    property bool fitted: true
    // The changed areas to outline, and which one is being looked at.
    property var regions: []
    property int focusedRegion: -1
    readonly property bool loading: sheetImage.status === Image.Loading

    // Markup: the tool in use ("pan" to look around and pick markups), and
    // the markup picked.
    property string tool: "pan"
    property int selectedMarkup: -1
    readonly property real pixelsPerPoint: points.width > 0 ? paper.width / points.width : 1
    readonly property point origin: Qt.point(paper.x - flick.contentX, paper.y - flick.contentY)
    readonly property bool promptOpen: textPrompt.visible
    // A calibration line was drawn, this many points long.
    signal calibrated(real length)

    function toSheet(x, y) {
        return Qt.point((x - origin.x) / pixelsPerPoint, (y - origin.y) / pixelsPerPoint)
    }

    function clampZoom(value) { return Math.max(minZoom, Math.min(maxZoom, value)) }

    onKindChanged: tileTimer.restart()

    function fit() {
        zoom = fitZoom
        renderZoom = zoom
        tileTimer.restart()
        fitted = true
        flick.contentX = 0
        flick.contentY = 0
    }

    // Zooms keeping the sheet point under (vx, vy) in the viewport still.
    function zoomAt(value, vx, vy) {
        value = clampZoom(value)
        if (value === zoom)
            return
        const px = (flick.contentX + vx - paper.x) / zoom
        const py = (flick.contentY + vy - paper.y) / zoom
        zoom = value
        fitted = false
        flick.contentX = Math.max(0, Math.min(px * zoom + paper.x - vx, flick.contentWidth - flick.width))
        flick.contentY = Math.max(0, Math.min(py * zoom + paper.y - vy, flick.contentHeight - flick.height))
    }

    // Brings a region (fractions of the sheet) to the middle of the view,
    // zoomed so it fills about half of it.
    function showRegion(region) {
        if (points.width <= 0)
            return
        const w = Math.max(region.width * points.width, 1), h = Math.max(region.height * points.height, 1)
        zoom = clampZoom(Math.min(width * 0.55 / w, height * 0.55 / h))
        renderZoom = zoom
        tileTimer.restart()
        fitted = false
        const cx = paper.x + (region.x + region.width / 2) * paper.width
        const cy = paper.y + (region.y + region.height / 2) * paper.height
        flick.contentX = Math.max(0, Math.min(cx - width / 2, flick.contentWidth - flick.width))
        flick.contentY = Math.max(0, Math.min(cy - height / 2, flick.contentHeight - flick.height))
    }

    function zoomBy(factor) { zoomAt(zoom * factor, width / 2, height / 2) }

    function pan(dx, dy) {
        flick.contentX = Math.max(0, Math.min(flick.contentX + dx, flick.contentWidth - flick.width))
        flick.contentY = Math.max(0, Math.min(flick.contentY + dy, flick.contentHeight - flick.height))
    }

    onZoomChanged: settle.restart()
    onFitZoomChanged: if (fitted) fit()

    Connections {
        target: backend
        function onSheetChanged() { Qt.callLater(viewer.fit) }
        function onDocumentsChanged() { Qt.callLater(viewer.fit) }
    }

    Timer {
        id: settle
        interval: 180
        onTriggered: {
            viewer.renderZoom = viewer.zoom
            viewer.updateTiles()
        }
    }

    Timer {
        id: tileTimer
        interval: 60
        onTriggered: viewer.updateTiles()
    }

    Flickable {
        id: flick
        anchors.fill: parent
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        // Drawing tools drag on the sheet; only the pan tool drags it around.
        interactive: viewer.tool === "pan"
        contentWidth: Math.max(width, paper.width + 2 * viewer.margin)
        contentHeight: Math.max(height, paper.height + 2 * viewer.margin)
        onContentXChanged: tileTimer.restart()
        onContentYChanged: tileTimer.restart()
        onWidthChanged: tileTimer.restart()
        onHeightChanged: tileTimer.restart()
        ScrollBar.vertical: ScrollBar {}
        ScrollBar.horizontal: ScrollBar {}

        Rectangle {
            id: paper
            color: "white"
            width: viewer.points.width * viewer.zoom
            height: viewer.points.height * viewer.zoom
            x: Math.max(viewer.margin, (flick.contentWidth - width) / 2)
            y: Math.max(viewer.margin, (flick.contentHeight - height) / 2)

            Image {
                id: sheetImage
                anchors.fill: parent
                source: backend.sheetCount > 0 && viewer.kind !== ""
                    ? "image://sheets/" + viewer.kind + "/" + backend.generation + "/" + backend.sheet : ""
                sourceSize: viewer.tiled
                    ? (viewer.fullSize.width >= viewer.fullSize.height
                        ? Qt.size(viewer.baseEdge, Math.round(viewer.baseEdge * viewer.fullSize.height / viewer.fullSize.width))
                        : Qt.size(Math.round(viewer.baseEdge * viewer.fullSize.width / viewer.fullSize.height), viewer.baseEdge))
                    : viewer.fullSize
                asynchronous: true
                retainWhileLoading: true
                // Full sheets are large; keep only the one on screen.
                cache: false
                smooth: true
                mipmap: true
            }

            Repeater {
                model: viewer.tiles
                delegate: Image {
                    required property var modelData
                    readonly property real scale: paper.width / viewer.fullSize.width
                    x: modelData.x * scale
                    y: modelData.y * scale
                    width: modelData.w * scale
                    height: modelData.h * scale
                    source: "image://sheets/" + viewer.kind + "/" + backend.generation + "/" + backend.sheet + "/"
                        + viewer.fullSize.width + "x" + viewer.fullSize.height + "/"
                        + [modelData.x, modelData.y, modelData.w, modelData.h].join(",")
                    asynchronous: true
                    smooth: true
                }
            }

            Repeater {
                model: viewer.regions
                delegate: Rectangle {
                    required property var modelData
                    required property int index
                    readonly property bool focused: index === viewer.focusedRegion
                    x: modelData.x * paper.width - 4
                    y: modelData.y * paper.height - 4
                    width: modelData.width * paper.width + 8
                    height: modelData.height * paper.height + 8
                    color: focused ? Qt.rgba(0.85, 0.56, 0.02, 0.06) : "transparent"
                    border.color: "#D98E04"
                    border.width: focused ? 3 : 1.5
                    opacity: focused || viewer.focusedRegion < 0 ? 1 : 0.6
                    radius: Math.min(4, theme.radius)
                }
            }
        }

        // A mouse wheel zooms at the pointer, as in every drawing viewer;
        // a touchpad's two-finger scroll pans, and zooms with Ctrl held.
        WheelHandler {
            target: null
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            onWheel: (event) => {
                const p = viewer.mapFromItem(flick.contentItem, point.position.x, point.position.y)
                if (event.pixelDelta.x !== 0 || event.pixelDelta.y !== 0) {
                    if (event.modifiers & Qt.ControlModifier)
                        viewer.zoomAt(viewer.zoom * Math.pow(1.01, event.pixelDelta.y), p.x, p.y)
                    else
                        viewer.pan(-event.pixelDelta.x, -event.pixelDelta.y)
                } else if (event.modifiers & Qt.ShiftModifier) {
                    viewer.pan(-event.angleDelta.y, 0)
                } else {
                    viewer.zoomAt(viewer.zoom * Math.pow(1.0015, event.angleDelta.y), p.x, p.y)
                }
            }
        }

        PinchHandler {
            target: null
            property real startZoom: 1
            onActiveChanged: if (active) startZoom = viewer.zoom
            onActiveScaleChanged: {
                const p = viewer.mapFromItem(flick.contentItem, centroid.position.x, centroid.position.y)
                viewer.zoomAt(startZoom * activeScale, p.x, p.y)
            }
        }
    }

    MarkupLayer {
        id: layer
        anchors.fill: flick
        markups: markupSet
        sheet: backend.sheet
        sheetSize: viewer.points
        origin: viewer.origin
        pixelsPerPoint: viewer.pixelsPerPoint
        selected: viewer.selectedMarkup
    }

    // Picks, moves, and draws markups. A press that misses every markup in
    // the pan tool falls through to the sheet below, which pans.
    MouseArea {
        id: pointer
        anchors.fill: flick
        hoverEnabled: true
        preventStealing: true
        property bool moving: false
        property bool overMarkup: false
        property point start
        cursorShape: viewer.tool !== "pan" ? Qt.CrossCursor : overMarkup ? Qt.PointingHandCursor : Qt.ArrowCursor

        function hitAt(mouse) {
            return markupSet.hitTest(backend.sheet, viewer.toSheet(mouse.x, mouse.y), 8 / viewer.pixelsPerPoint)
        }

        onPressed: (mouse) => {
            const p = viewer.toSheet(mouse.x, mouse.y)
            if (viewer.tool === "pan") {
                viewer.selectedMarkup = hitAt(mouse)
                if (viewer.selectedMarkup < 0) {
                    mouse.accepted = false
                    return
                }
                moving = true
                start = p
            } else if (viewer.tool === "text") {
                textPrompt.sheetPoint = p
                textPrompt.x = Math.min(mouse.x, viewer.width - textPrompt.width - 8)
                textPrompt.y = Math.max(8, mouse.y - textPrompt.height / 2)
                textPrompt.open()
            } else {
                start = p
                layer.draft = { type: viewer.tool, points: [p, p] }
            }
        }
        onPositionChanged: (mouse) => {
            if (!pressed) {
                overMarkup = viewer.tool === "pan" && hitAt(mouse) >= 0
                return
            }
            const p = viewer.toSheet(mouse.x, mouse.y)
            if (moving)
                layer.moveOffset = Qt.point(p.x - start.x, p.y - start.y)
            else if (layer.draft.points)
                layer.draft = { type: viewer.tool, points: [start, p] }
        }
        onReleased: {
            if (moving) {
                markupSet.move(backend.sheet, viewer.selectedMarkup, layer.moveOffset.x, layer.moveOffset.y)
            } else if (layer.draft.points) {
                const points = layer.draft.points
                if (viewer.tool === "calibrate") {
                    const length = Math.hypot(points[1].x - points[0].x, points[1].y - points[0].y)
                    if (length > 1)
                        viewer.calibrated(length)
                } else {
                    markupSet.add(backend.sheet, viewer.tool, points)
                }
            }
            reset()
        }
        onCanceled: reset()

        function reset() {
            moving = false
            layer.moveOffset = Qt.point(0, 0)
            layer.draft = ({})
        }
    }

    // Where a text markup's words are typed, at the click.
    Popup {
        id: textPrompt
        property point sheetPoint
        padding: 6
        Material.roundedScale: theme.radius
        background: Rectangle { color: "#17171a"; radius: Math.min(6, theme.radius); border.color: theme.accent }
        onOpened: {
            noteField.text = ""
            noteField.forceActiveFocus()
        }

        TextField {
            id: noteField
            width: 280
            placeholderText: qsTr("Note, then Enter")
            onAccepted: {
                markupSet.add(backend.sheet, "text", [textPrompt.sheetPoint], text)
                textPrompt.close()
            }
        }
    }

    BusyIndicator {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 12
        width: 36
        height: 36
        running: viewer.loading
    }
}
