// Wallpapers per screen: drop a picture on a screen, or use one everywhere (docs/pages-contract.md, "Wallpapers").
// The root's objectName is widget:theme-wallpaper. The functions below are the ones the gate calls (untyped parameters);
// the page's own mouse, drag and drop handlers call the same ones.
// M3 scaffolding: a stub that shows the page title and "not ready"; its backend refuses everything.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    objectName: "widget:theme-wallpaper"

    required property Controller panel
    required property PageItem page

    readonly property var screens: backend.screens

    ThemeWallpaperBackend {
        id: backend
        panel: root.panel
        page: root.page
    }

    function refresh() { return backend.refresh() }
    function screenAt(x, y, width, height) { return backend.screenAt(x, y, width, height) }
    function dropPicture(path, desc) { return backend.dropPicture(path, desc) }
    function useEverywhere(path) { return backend.useEverywhere(path) }

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
            objectName: "notReady:theme-wallpaper"
            Layout.fillWidth: true
            text: qsTr("not ready")
            opacity: 0.75
        }
    }
}
