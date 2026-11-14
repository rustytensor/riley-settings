// Displays: drag screens into place, apply with a 15 s revert (docs/pages-contract.md, "Displays").
// The root's objectName is widget:displays. The functions below are the ones the gate calls (untyped parameters);
// the page's own mouse, drag and drop handlers call the same ones.
// A port of Riley's displays.py: the arrangement drawn to scale (drag a screen; it snaps to its neighbours' edges),
// one box of settings per screen, Apply that Hyprland checks first, and "Keep these display settings?" that goes
// back by itself after 15 seconds.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    objectName: "widget:displays"

    required property Controller panel
    required property PageItem page

    readonly property bool dirty: backend.dirty
    readonly property bool pending: backend.pending
    readonly property int keepSeconds: backend.keepSeconds
    readonly property int secondsLeft: backend.secondsLeft
    readonly property var screens: backend.screens

    DisplaysBackend {
        id: backend
        panel: root.panel
        page: root.page
    }

    Component.onCompleted: backend.load()

    function load() { return backend.load() }
    function dragMove(desc, dx, dy) { return backend.dragMove(String(desc), Number(dx), Number(dy)) }
    function dragEnd(desc) { return backend.dragEnd(String(desc)) }
    function setScreenField(desc, field, text) { return backend.setScreenField(String(desc), String(field), String(text)) }
    function reset() { return backend.reset() }
    function apply() { return backend.apply() }
    function keep() { return backend.keep() }
    function revert() { return backend.revert() }

    function info(desc) {
        const list = root.screens
        for (let i = 0; i < list.length; ++i)
            if (list[i].desc === desc)
                return list[i]
        return null
    }
    function shortName(desc) {
        const parts = String(desc).split(" ")
        return parts.length >= 2 ? parts[parts.length - 2] : String(desc)
    }

    readonly property var rotations: [qsTr("Normal"), qsTr("90°"), qsTr("180°"), qsTr("270°")]

    ScrollView {
        id: scroll
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: scroll.availableWidth
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                Layout.margins: 16
                Layout.bottomMargin: 0
                spacing: 8

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
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
                        opacity: 0.7
                        font.pixelSize: 12
                    }
                }

                Button {
                    objectName: "displays:reset"
                    text: qsTr("Reset")
                    enabled: !root.pending && root.dirty
                    onClicked: root.reset()
                }
                Button {
                    objectName: "displays:apply"
                    text: qsTr("Apply")
                    highlighted: true
                    enabled: !root.pending
                    onClicked: root.apply()
                }
            }

            // "Keep these display settings?" while the clock runs.
            Frame {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                visible: root.pending

                RowLayout {
                    anchors.fill: parent
                    spacing: 8
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Keep these display settings? Going back to the previous settings in %1 seconds.")
                                  .arg(root.secondsLeft)
                        wrapMode: Text.WordWrap
                    }
                    Button {
                        objectName: "displays:revert"
                        visible: root.pending
                        text: qsTr("Go back")
                        onClicked: root.revert()
                    }
                    Button {
                        objectName: "displays:keep"
                        visible: root.pending
                        text: qsTr("Keep")
                        highlighted: true
                        onClicked: root.keep()
                    }
                }
            }

            // The arrangement, drawn to scale (displays.py Arrangement.view: fit with a 20 px margin, centred).
            Frame {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                padding: 0

                Item {
                    id: area
                    implicitHeight: 300
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 300
                    clip: true

                    // Kept while a drag runs, so the drawing does not rescale under the pointer.
                    property var frozen: null
                    readonly property var live: {
                        const ms = root.screens.filter(s => s.shown && s.w > 0)
                        if (ms.length === 0)
                            return { s: 1, ox: 0, oy: 0 }
                        let x0 = Infinity, y0 = Infinity, x1 = -Infinity, y1 = -Infinity
                        for (const m of ms) {
                            x0 = Math.min(x0, m.px); y0 = Math.min(y0, m.py)
                            x1 = Math.max(x1, m.px + m.w); y1 = Math.max(y1, m.py + m.h)
                        }
                        const s = Math.min((width - 40) / Math.max(1, x1 - x0), (height - 40) / Math.max(1, y1 - y0))
                        return { s: s, ox: (width - (x1 - x0) * s) / 2 - x0 * s, oy: (height - (y1 - y0) * s) / 2 - y0 * s }
                    }
                    readonly property var view: frozen ?? live

                    Repeater {
                        model: backend.shownDescs

                        delegate: Rectangle {
                            id: screenItem
                            required property string modelData
                            required property int index
                            readonly property var data_: root.info(modelData)
                            readonly property int screenX: data_ ? data_.px : 0
                            readonly property int screenY: data_ ? data_.py : 0
                            objectName: "screen:" + modelData

                            x: area.view.ox + screenX * area.view.s
                            y: area.view.oy + screenY * area.view.s
                            width: Math.max(4, (data_ ? data_.w : 0) * area.view.s)
                            height: Math.max(4, (data_ ? data_.h : 0) * area.view.s)
                            radius: 8
                            color: mouseArea.pressed ? Qt.alpha(palette.highlight, 0.55) : Qt.alpha(palette.text, 0.10)
                            border.width: mouseArea.pressed ? 2.5 : 1.2
                            border.color: mouseArea.pressed ? palette.highlight : Qt.alpha(palette.text, 0.35)
                            z: mouseArea.pressed ? 1 : 0

                            Column {
                                anchors.centerIn: parent
                                spacing: 2
                                Label {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: String(root.screens.findIndex(s => s.desc === screenItem.modelData) + 1)
                                    font.bold: true
                                    font.pixelSize: Math.max(14, Math.min(34, screenItem.height * 0.28))
                                }
                                Label {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: root.shortName(screenItem.modelData)
                                    visible: implicitWidth < screenItem.width - 8
                                    font.pixelSize: Math.max(10, Math.min(14, screenItem.height * 0.12))
                                }
                            }

                            MouseArea {
                                id: mouseArea
                                anchors.fill: parent
                                property point start
                                onPressed: mouse => {
                                    area.frozen = area.live
                                    start = mapToItem(area, mouse.x, mouse.y)
                                }
                                onPositionChanged: mouse => {
                                    if (!pressed)
                                        return
                                    const p = mapToItem(area, mouse.x, mouse.y)
                                    root.dragMove(screenItem.modelData, (p.x - start.x) / area.view.s,
                                                  (p.y - start.y) / area.view.s)
                                }
                                onReleased: {
                                    root.dragEnd(screenItem.modelData)
                                    area.frozen = null
                                }
                                onCanceled: {
                                    root.dragEnd(screenItem.modelData)
                                    area.frozen = null
                                }
                            }
                        }
                    }
                }
            }

            // One box per screen (displays.py DisplaysPage.build): on or off, resolution and rate, scale, rotation.
            Repeater {
                model: backend.allDescs

                delegate: GroupBox {
                    id: box
                    required property string modelData
                    required property int index
                    readonly property var data_: root.info(modelData)
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    title: (index + 1) + "  " + modelData

                    GridLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        columns: 2
                        columnSpacing: 16

                        Label { Layout.fillWidth: true; text: qsTr("Use this screen") }
                        Switch {
                            checked: box.data_ ? box.data_.enabled === "true" : false
                            onToggled: root.setScreenField(box.modelData, "enabled", checked ? "true" : "false")
                        }

                        Label { Layout.fillWidth: true; text: qsTr("Resolution and refresh rate") }
                        ComboBox {
                            id: modeBox
                            Layout.preferredWidth: 220
                            readonly property var modes: {
                                const list = box.data_ ? box.data_.modes.slice() : []
                                const cur = box.data_ ? box.data_.mode : ""
                                if (cur !== "" && list.indexOf(cur) < 0)
                                    list.unshift(cur)
                                return list
                            }
                            model: modes
                            currentIndex: box.data_ ? modes.indexOf(box.data_.mode) : -1
                            onActivated: i => root.setScreenField(box.modelData, "mode", modes[i])
                        }

                        Label { Layout.fillWidth: true; text: qsTr("Scale") }
                        ComboBox {
                            Layout.preferredWidth: 220
                            readonly property var values: {
                                const list = backend.scales.slice()
                                const cur = box.data_ ? box.data_.scale : ""
                                if (cur !== "" && list.indexOf(cur) < 0)
                                    list.push(cur)
                                return list
                            }
                            model: values.map(v => isNaN(Number(v)) ? v : Math.round(Number(v) * 100) + " %")
                            currentIndex: box.data_ ? values.indexOf(box.data_.scale) : -1
                            onActivated: i => root.setScreenField(box.modelData, "scale", values[i])
                        }

                        Label { Layout.fillWidth: true; text: qsTr("Rotation") }
                        ComboBox {
                            Layout.preferredWidth: 220
                            model: root.rotations
                            currentIndex: box.data_ ? Number(box.data_.transform) % 4 : 0
                            onActivated: i => root.setScreenField(box.modelData, "transform", String(i))
                        }

                        Label { Layout.fillWidth: true; text: qsTr("Colour depth") }
                        ComboBox {
                            Layout.preferredWidth: 220
                            readonly property var depths: {
                                const list = box.data_ ? box.data_.depths.slice() : ["8"]
                                const cur = box.data_ ? box.data_.bitdepth : "8"
                                if (list.indexOf(cur) < 0)
                                    list.push(cur)
                                return list
                            }
                            model: depths.map(d => qsTr("%1-bit").arg(d))
                            enabled: depths.length > 1
                            currentIndex: box.data_ ? depths.indexOf(box.data_.bitdepth) : 0
                            onActivated: i => root.setScreenField(box.modelData, "bitdepth", depths[i])
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: 16 }
        }
    }
}
