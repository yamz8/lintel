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
