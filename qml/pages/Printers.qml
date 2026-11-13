// Printers: CUPS queues, jobs, your options and default, through the CUPS command-line tools (docs/pages-contract.md, "Printers").
// The root's objectName is widget:printers. The functions below are the ones the gate calls (untyped parameters);
// the page's own mouse, drag and drop handlers call the same ones.
// M3 scaffolding: a stub that shows the page title and "not ready"; its backend refuses everything.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    objectName: "widget:printers"

    required property Controller panel
    required property PageItem page

    readonly property var printers: backend.printers
    readonly property var jobs: backend.jobs

    PrintersBackend {
        id: backend
        panel: root.panel
        page: root.page
    }

    function refresh() { return backend.refresh() }
    function printTestPage(queue) { return backend.printTestPage(queue) }
    function cancelJob(requestId) { return backend.cancelJob(requestId) }
    function setOption(queue, option, value) { return backend.setOption(queue, option, value) }
    function userOption(queue, option) { return backend.userOption(queue, option) }
    function setDefault(queue) { return backend.setDefault(queue) }
    function openSetupTool() { return backend.openSetupTool() }

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
            objectName: "notReady:printers"
            Layout.fillWidth: true
            text: qsTr("not ready")
            opacity: 0.75
        }
    }
}
