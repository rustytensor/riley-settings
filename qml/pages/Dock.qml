// Dock: pins by drag and drop, the app list, the dock's screens (docs/pages-contract.md, "Dock").
// The root's objectName is widget:dock. The functions below are the ones the gate calls (untyped parameters);
// the page's own mouse, drag and drop handlers call the same ones.
// M3 scaffolding: a stub that shows the page title and "not ready"; its backend refuses everything.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    objectName: "widget:dock"

    required property Controller panel
    required property PageItem page

    readonly property var pins: backend.pins
    readonly property var appIds: backend.appIds
    readonly property var apps: backend.apps
    readonly property var screens: backend.screens

    DockBackend {
        id: backend
        panel: root.panel
        page: root.page
    }

    function refresh() { return backend.refresh() }
    function dropOnDock(value, x) { return backend.dropOnDock(value, x) }
    function dropOnRemove(value) { return backend.dropOnRemove(value) }
    function setScreens(descs) { return backend.setScreens(descs) }

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
            objectName: "notReady:dock"
            Layout.fillWidth: true
            text: qsTr("not ready")
            opacity: 0.75
        }
    }
}
