// Keyboard and Mouse: the page's options are M2 controls; helpers (a layout picker, a repeat test box) end in their commit functions (docs/pages-contract.md, "Keyboard and Mouse").
// The root's objectName is widget:keyboard-mouse. The functions below are the ones the gate calls (untyped parameters);
// the page's own mouse, drag and drop handlers call the same ones.
// M3 scaffolding: a stub that shows the page title and "not ready"; its backend refuses everything.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    objectName: "widget:keyboard-mouse"

    required property Controller panel
    required property PageItem page

    KeyboardMouseBackend {
        id: backend
        panel: root.panel
        page: root.page
    }

    function refresh() { return backend.refresh() }

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
            objectName: "notReady:keyboard-mouse"
            Layout.fillWidth: true
            text: qsTr("not ready")
            opacity: 0.75
        }
    }
}
