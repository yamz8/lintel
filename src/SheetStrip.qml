import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// Every sheet in the set as a thumbnail, in order.
ListView {
    id: strip
    clip: true
    model: backend.sheetCount
    currentIndex: backend.sheet
    spacing: 4
    topMargin: 8
    bottomMargin: 8
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar {}
    highlightMoveDuration: 120

    onCurrentIndexChanged: positionViewAtIndex(currentIndex, ListView.Contain)

    delegate: Item {
        id: entry
        required property int index
        readonly property bool selected: index === backend.sheet
        // Reading the revisions re-evaluates this as sets open and sheets
        // finish comparing.
        readonly property string status: backend.generation >= 0 && backend.analysisRevision >= 0
            ? backend.sheetStatus(index) : ""
        width: ListView.view.width
        height: thumb.height + label.implicitHeight + 22

        Rectangle {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            radius: Math.min(4, theme.radius)
            color: entry.selected ? Qt.alpha(theme.accent, 0.16) : hover.hovered ? "#1c1c20" : "transparent"
            border.color: entry.selected ? theme.accent : "transparent"
            border.width: 1
        }

        Rectangle {
            id: thumb
            anchors.horizontalCenter: parent.horizontalCenter
            y: 8
            color: "white"
            readonly property size points: backend.generation >= 0 ? backend.sizeOf(entry.index) : Qt.size(0, 0)
            width: entry.width - 40
            height: points.width > 0 ? width * points.height / points.width : width

            // Sheets that came through unchanged step back, so the eye goes
            // to the ones that need looking at.
            opacity: entry.status === "unchanged" ? 0.55 : 1

            Image {
                anchors.fill: parent
                source: "image://sheets/current/" + backend.generation + "/" + entry.index
                sourceSize.width: Math.round(parent.width * Screen.devicePixelRatio)
                asynchronous: true
                smooth: true
                mipmap: true
            }
        }

        // How many markups the sheet has.
        Rectangle {
            readonly property int count: markupSet.revision >= 0 ? markupSet.count(entry.index) : 0
            visible: count > 0
            anchors.right: thumb.right
            anchors.top: thumb.top
            anchors.margins: 4
            width: Math.max(height, markCount.implicitWidth + 8)
            height: markCount.implicitHeight + 2
            radius: height / 2
            color: "#1E6BFF"

            Label {
                id: markCount
                anchors.centerIn: parent
                text: parent.count
                color: "white"
                font.pixelSize: 10
                font.bold: true
            }
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: thumb.bottom
            anchors.topMargin: 5
            spacing: 6

            Label {
                id: label
                text: backend.generation >= 0 ? backend.sheetLabel(entry.index) : ""
                color: entry.selected ? theme.accent : "#b8b8c0"
                font.pixelSize: 12
                elide: Text.ElideMiddle
                width: Math.min(implicitWidth, entry.width - 70)
            }

            Rectangle {
                visible: entry.status === "new" || entry.status === "changed"
                width: tag.implicitWidth + 8
                height: tag.implicitHeight + 2
                radius: Math.min(3, theme.radius)
                color: entry.status === "new" ? "#1FA34A" : "#D98E04"
                anchors.verticalCenter: label.verticalCenter

                Label {
                    id: tag
                    anchors.centerIn: parent
                    text: entry.status === "new" ? qsTr("New") : qsTr("Changed")
                    color: entry.status === "new" ? "white" : "black"
                    font.pixelSize: 10
                    font.bold: true
                }
            }
        }

        HoverHandler { id: hover }
        TapHandler { onTapped: backend.sheet = entry.index }
    }
}
