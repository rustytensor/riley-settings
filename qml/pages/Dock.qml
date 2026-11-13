// Dock: pins by drag and drop, the app list, the dock's screens (docs/pages-contract.md, "Dock").
// The root's objectName is widget:dock. The functions below are the ones the gate calls (untyped parameters);
// the page's own mouse, drag and drop handlers call the same ones.
// A port of Riley's settings/riley_settings/dock.py: drag an icon along the dock to move it, drag an app onto the
// dock to add it (or double-click it), drag a dock icon onto Remove to take it off. The dock's other options are
// M2 controls (PageOptions).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    objectName: "widget:dock"

    required property Controller panel
    required property PageItem page

    readonly property list<string> pins: backend.pins
    readonly property list<string> appIds: backend.appIds
    readonly property var apps: backend.apps
    readonly property var screens: backend.screens

    // What is being dragged now ("pin:<id>", "app:<id>"), or "".
    property string dragging: ""
    property bool loaded: false

    DockBackend {
        id: backend
        panel: root.panel
        page: root.page
    }

    function refresh() { return backend.refresh() }
    function dropOnDock(value, x) { return backend.dropOnDock(value, x) }
    function dropOnRemove(value) { return backend.dropOnRemove(value) }
    function setScreens(descs) { return backend.setScreens(descs) }

    // Showing the page reads the apps and the screens in use (no writes); a hidden page runs no command.
    onVisibleChanged: if (visible && !loaded) { loaded = true; backend.refresh() }
    Component.onCompleted: if (visible && !loaded) { loaded = true; backend.refresh() }

    // An icon that can be dragged: carries `value` to the strip's or Remove's drop area.
    component DragTile: Item {
        id: tile

        property string value
        property string label
        property bool showLabel: false
        property bool faded: false
        signal doubleClicked()

        readonly property string dragValue: value
        implicitWidth: 48
        implicitHeight: showLabel ? 76 : 48
        opacity: faded ? 0.45 : 1

        Item {
            id: ghost
            width: 48
            height: 48
            Drag.active: area.drag.active
            Drag.source: tile
            Drag.hotSpot.x: 24
            Drag.hotSpot.y: 24
            z: 10

            Rectangle {
                anchors.fill: parent
                anchors.margins: 4
                radius: 10
                color: Qt.hsla((tile.value.length * 37 % 360) / 360, 0.45, 0.45, 1)
                border.color: Qt.rgba(1, 1, 1, 0.25)

                Label {
                    anchors.centerIn: parent
                    text: tile.label.length > 0 ? tile.label.charAt(0).toUpperCase() : "?"
                    color: "white"
                    font.bold: true
                    font.pixelSize: 18
                }
            }
        }

        Label {
            visible: tile.showLabel
            anchors.top: parent.top
            anchors.topMargin: 50
            anchors.horizontalCenter: parent.horizontalCenter
            width: 84
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            font.pixelSize: 11
            text: tile.label
        }

        MouseArea {
            id: area
            anchors.fill: parent
            drag.target: ghost
            ToolTip.visible: containsMouse && !drag.active
            ToolTip.text: tile.label
            hoverEnabled: true
            onPressed: root.dragging = tile.value
            onReleased: {
                if (drag.active)
                    ghost.Drag.drop()
                ghost.x = 0
                ghost.y = 0
                root.dragging = ""
            }
            onCanceled: {
                ghost.x = 0
                ghost.y = 0
                root.dragging = ""
            }
            onDoubleClicked: tile.doubleClicked()
        }
    }

    ScrollView {
        id: scroll
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: scroll.availableWidth
            spacing: 12

            Label {
                Layout.fillWidth: true
                text: root.page ? root.page.title : ""
                font.bold: true
                font.pixelSize: 16
            }

            Label {
                Layout.fillWidth: true
                text: root.page ? root.page.help : ""
                wrapMode: Text.WordWrap
                opacity: 0.75
            }

            // The dock: a shelf of 48 px icons, and Remove above it while a dock icon is dragged.
            ColumnLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 10

                Rectangle {
                    id: removeZone
                    objectName: "dock:remove"
                    Layout.alignment: Qt.AlignHCenter
                    implicitWidth: removeLabel.implicitWidth + 44
                    implicitHeight: 40
                    radius: 12
                    opacity: root.dragging.startsWith("pin:") ? 1 : 0.35
                    color: removeDrop.containsDrag ? Qt.rgba(0.9, 0.2, 0.2, 0.18) : "transparent"
                    border.color: Qt.rgba(0.9, 0.2, 0.2, 0.7)
                    border.width: 1

                    Label {
                        id: removeLabel
                        anchors.centerIn: parent
                        text: qsTr("Drop here to remove from the dock")
                        color: Qt.rgba(0.9, 0.25, 0.25, 1)
                    }

                    DropArea {
                        id: removeDrop
                        anchors.fill: parent
                        onDropped: (drop) => {
                            const v = drop.source && drop.source.dragValue !== undefined ? drop.source.dragValue : drop.text
                            if (root.dropOnRemove(v))
                                drop.accept()
                        }
                    }
                }

                Rectangle {
                    id: shelf
                    Layout.alignment: Qt.AlignHCenter
                    implicitWidth: Math.max(strip.width, emptyLabel.implicitWidth) + 24
                    implicitHeight: 64
                    radius: 18
                    color: Qt.rgba(scroll.palette.base.r, scroll.palette.base.g, scroll.palette.base.b, 0.78)
                    border.color: Qt.rgba(scroll.palette.highlight.r, scroll.palette.highlight.g, scroll.palette.highlight.b, 0.35)

                    Label {
                        id: emptyLabel
                        anchors.centerIn: parent
                        visible: root.pins.length === 0
                        text: qsTr("Drag apps here")
                        opacity: 0.7
                    }

                    Row {
                        id: strip
                        objectName: "dock:strip"
                        x: 12
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 0

                        Repeater {
                            model: root.pins

                            delegate: DragTile {
                                required property string modelData
                                objectName: "pin:" + modelData
                                width: backend.pinWidth
                                height: 48
                                value: "pin:" + modelData
                                label: {
                                    for (const a of root.apps)
                                        if (a.id === modelData)
                                            return a.name
                                    return modelData
                                }
                            }
                        }
                    }

                    // Where the drop would land: a thin bar between the icons.
                    Rectangle {
                        visible: stripDrop.containsDrag && root.pins.length > 0
                        width: 4
                        height: 40
                        radius: 2
                        anchors.verticalCenter: parent.verticalCenter
                        color: scroll.palette.highlight
                        x: strip.x + backend.insertIndex(stripDrop.dragX - strip.x) * backend.pinWidth - 2
                    }

                    DropArea {
                        id: stripDrop
                        anchors.fill: parent
                        property real dragX: drag.x
                        onDropped: (drop) => {
                            const v = drop.source && drop.source.dragValue !== undefined ? drop.source.dragValue : drop.text
                            const p = mapToItem(strip, drop.x, drop.y)
                            if (root.dropOnDock(v, p.x))
                                drop.accept()
                        }
                    }
                }
            }

            Label {
                text: qsTr("Apps")
                font.bold: true
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Faded apps are on the dock already. Drag one onto the dock, or double-click it to add it at the end.")
                wrapMode: Text.WordWrap
                opacity: 0.75
            }

            TextField {
                id: search
                Layout.preferredWidth: 280
                placeholderText: qsTr("Search apps")
            }

            Flow {
                Layout.fillWidth: true
                spacing: 8

                Repeater {
                    model: root.apps

                    delegate: DragTile {
                        required property var modelData
                        objectName: "app:" + modelData.id
                        visible: search.text === "" || modelData.name.toLowerCase().indexOf(search.text.toLowerCase()) >= 0
                        width: 88
                        showLabel: true
                        value: "app:" + modelData.id
                        label: modelData.name
                        faded: modelData.pinned
                        onDoubleClicked: root.dropOnDock(value, 1e9)
                    }
                }
            }

            Label {
                text: qsTr("Screens")
                font.bold: true
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Choose screens by their descriptions, not their ports. If none match, the dock uses the top-left screen.")
                wrapMode: Text.WordWrap
                opacity: 0.75
            }

            Label {
                visible: root.screens.length === 0
                text: qsTr("No connected screens reported")
                opacity: 0.75
            }

            Repeater {
                model: root.screens

                delegate: CheckBox {
                    required property var modelData
                    objectName: "dockScreen:" + modelData.desc
                    text: modelData.desc + "  (" + modelData.name + ")"
                    checked: modelData.checked
                    enabled: modelData.desc !== ""
                    onToggled: backend.setScreenChecked(modelData.desc, checked)
                }
            }

            PageOptions {
                Layout.fillWidth: true
                page: root.page
            }
        }
    }
}
