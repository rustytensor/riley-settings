// Displays: drag screens into place, apply with a 15 s revert (docs/pages-contract.md, "Displays").
// The root's objectName is widget:displays. The functions below are the ones the gate calls (untyped parameters);
// the page's own mouse, drag and drop handlers call the same ones.
// M3 scaffolding: a stub that shows the page title and "not ready"; its backend refuses everything.
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

    function load() { return backend.load() }
    function dragMove(desc, dx, dy) { return backend.dragMove(desc, dx, dy) }
    function dragEnd(desc) { return backend.dragEnd(desc) }
    function setScreenField(desc, field, text) { return backend.setScreenField(desc, field, text) }
    function reset() { return backend.reset() }
    function apply() { return backend.apply() }
    function keep() { return backend.keep() }
    function revert() { return backend.revert() }

    ColumnLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 8

        Label {
            Layout.fillWidth: true
            text: root.page ? root.page.title : ""
            font.bold: true
            font.pixelSize: 16
        }

        Label {
            objectName: "notReady:displays"
            Layout.fillWidth: true
            text: qsTr("not ready")
            opacity: 0.75
        }
    }
}
