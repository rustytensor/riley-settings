// Desktop: the folder boxes of riley-desktop, a port of Riley's layout.py (docs/pages-contract.md, "Desktop").
// The root's objectName is widget:desktop. The functions below are the ones the gate calls (untyped parameters);
// the page's own mouse, drag and drop handlers call the same ones.
// M3 scaffolding: a stub that shows the page title and "not ready"; its backend refuses everything.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    objectName: "widget:desktop"

    required property Controller panel
    required property PageItem page

    readonly property var shown: backend.shown

    DesktopBackend {
        id: backend
        panel: root.panel
        page: root.page
    }

    function reload() { return backend.reload() }
    function addFolder(path) { return backend.addFolder(path) }
    function setHidden(folder, hidden) { return backend.setHidden(folder, hidden) }
    function removeBox(folder) { return backend.removeBox(folder) }

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
            objectName: "notReady:desktop"
            Layout.fillWidth: true
            text: qsTr("not ready")
            opacity: 0.75
        }
    }
}
