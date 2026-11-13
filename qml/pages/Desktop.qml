// Desktop: the folder boxes of riley-desktop, a port of Riley's layout.py and desktop_page.py
// (docs/pages-contract.md, "Desktop"). The root's objectName is widget:desktop. The functions below are the ones the
// gate calls (untyped parameters); the page's switches, buttons and folder dialog call the same ones.
// The switch for all boxes and the icon size are the page's scalar options, drawn as M2 controls (PageOptions).
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
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

    // Showing the page reads desktop.json, ~/Desktop and the screens again (Riley's build()).
    onVisibleChanged: if (visible) reload()
    Component.onCompleted: if (visible) reload()

    FolderDialog {
        id: folderDialog
        title: qsTr("A folder for a new box")
        onAccepted: root.addFolder(backend.localPath(selectedFolder))
    }

    ScrollView {
        id: scroll
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: scroll.availableWidth
            spacing: 18

            PageOptions {
                Layout.fillWidth: true
                page: root.page
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Boxes")
                    font.bold: true
                }

                Button {
                    objectName: "desktop:add"
                    text: qsTr("Add Folder…")
                    onClicked: folderDialog.open()
                }
            }

            Label {
                Layout.fillWidth: true
                visible: backend.boxes.length === 0
                text: qsTr("No boxes yet.")
                opacity: 0.75
            }

            Repeater {
                model: backend.boxes

                delegate: RowLayout {
                    id: boxRow

                    required property var modelData

                    objectName: "box:" + modelData.folder
                    Layout.fillWidth: true
                    spacing: 12

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            Layout.fillWidth: true
                            text: boxRow.modelData.title
                            elide: Text.ElideRight
                        }

                        Label {
                            Layout.fillWidth: true
                            text: boxRow.modelData.folder
                                  + (boxRow.modelData.onScreen ? "" : qsTr(" · a screen that is not connected"))
                                  + (boxRow.modelData.missing ? qsTr(" · folder is missing") : "")
                            elide: Text.ElideMiddle
                            opacity: 0.7
                            font.pixelSize: 12
                        }
                    }

                    Switch {
                        checked: !boxRow.modelData.hidden
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Show this box")
                        onToggled: {
                            root.setHidden(boxRow.modelData.folder, !checked)
                            checked = Qt.binding(() => !boxRow.modelData.hidden)   // what desktop.json says now
                        }
                    }

                    Button {
                        visible: !boxRow.modelData.auto
                        text: qsTr("Remove")
                        flat: true
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Remove this box (the folder stays)")
                        onClicked: root.removeBox(boxRow.modelData.folder)
                    }
                }
            }
        }
    }
}
