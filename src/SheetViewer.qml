import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

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
    readonly property real maxZoom: Math.max(fitZoom * 2, 6)
    // Logical pixels per PDF point.
    property real zoom: 1
    property real renderZoom: 1
    property bool fitted: true
    // The changed areas to outline, and which one is being looked at.
    property var regions: []
    property int focusedRegion: -1
    readonly property bool loading: sheetImage.status === Image.Loading

    function clampZoom(value) { return Math.max(minZoom, Math.min(maxZoom, value)) }

    function fit() {
        zoom = fitZoom
        renderZoom = zoom
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
        onTriggered: viewer.renderZoom = viewer.zoom
    }

    Flickable {
        id: flick
        anchors.fill: parent
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        contentWidth: Math.max(width, paper.width + 2 * viewer.margin)
        contentHeight: Math.max(height, paper.height + 2 * viewer.margin)
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
                sourceSize: Qt.size(Math.round(viewer.points.width * viewer.renderZoom * Screen.devicePixelRatio),
                                    Math.round(viewer.points.height * viewer.renderZoom * Screen.devicePixelRatio))
                asynchronous: true
                retainWhileLoading: true
                // Full sheets are large; keep only the one on screen.
                cache: false
                smooth: true
                mipmap: true
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

    BusyIndicator {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 12
        width: 36
        height: 36
        running: viewer.loading
    }
}
