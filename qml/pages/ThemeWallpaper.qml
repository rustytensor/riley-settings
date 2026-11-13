// Wallpapers per screen: drop a picture on a screen, or use one everywhere (docs/pages-contract.md, "Wallpapers").
// The root's objectName is widget:theme-wallpaper. The functions below are the ones the gate calls (untyped parameters);
// the page's own mouse, drag and drop handlers call the same ones.
// A port of the wallpaper part of Riley's GTK panel's theme_wallpaper.py (riley 2364a47): your screens as they are
// arranged, each showing its picture; drag a picture onto one screen (from below or from a file manager), or click a
// picture to use it on every screen. The theme cards are not part of M3.
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

Item {
    id: root
    objectName: "widget:theme-wallpaper"

    required property Controller panel
    required property PageItem page

    readonly property var screens: backend.screens
    property bool loaded: false
    property string hover: ""

    ThemeWallpaperBackend {
        id: backend
        panel: root.panel
        page: root.page
    }

    function refresh() { return backend.refresh() }
    function screenAt(x, y, width, height) { return backend.screenAt(x, y, width, height) }
    function dropPicture(path, desc) { return backend.dropPicture(path, desc) }
    function useEverywhere(path) { return backend.useEverywhere(path) }

    // Showing the page reads the screens in use (no writes); a hidden page runs no command.
    onVisibleChanged: if (visible && !loaded) { loaded = true; backend.refresh() }
    Component.onCompleted: if (visible && !loaded) { loaded = true; backend.refresh() }

    function droppedPath(drop) {
        if (drop.hasUrls && drop.urls.length > 0)
            return backend.localPath(drop.urls[0])
        if (drop.source && drop.source.dragValue !== undefined)
            return drop.source.dragValue
        return backend.localPath(drop.text)
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
                text: qsTr("Drag a picture onto a screen to use it there; click a picture to use it on every screen. Pictures dragged from Files work too.")
                wrapMode: Text.WordWrap
                opacity: 0.75
            }

            // The screens as they are arranged (Riley's ScreenDrop).
            Rectangle {
                id: drawing
                objectName: "wallpaper:screens"
                Layout.fillWidth: true
                Layout.preferredHeight: 320
                radius: 8
                color: Qt.rgba(0, 0, 0, 0.12)
                border.color: Qt.rgba(scroll.palette.text.r, scroll.palette.text.g, scroll.palette.text.b, 0.15)

                Label {
                    anchors.centerIn: parent
                    visible: root.screens.length === 0
                    text: qsTr("No screens reported")
                    opacity: 0.7
                }

                Repeater {
                    model: {
                        root.screens   // drawn again when the screens or their pictures change
                        return drawing.width > 0 && drawing.height > 0 ? backend.rects(drawing.width, drawing.height) : []
                    }

                    delegate: Item {
                        id: screenBox
                        required property var modelData
                        objectName: "wallpaperScreen:" + modelData.desc
                        x: modelData.x
                        y: modelData.y
                        width: modelData.w
                        height: modelData.h
                        readonly property bool hot: root.hover === modelData.desc

                        Rectangle {
                            id: face
                            anchors.fill: parent
                            anchors.margins: 2
                            radius: 8
                            clip: true
                            color: Qt.rgba(1, 1, 1, 0.08)
                            border.width: screenBox.hot ? 3 : 1.2
                            border.color: screenBox.hot ? scroll.palette.highlight : Qt.rgba(1, 1, 1, 0.3)

                            Image {
                                anchors.fill: parent
                                anchors.margins: face.border.width
                                source: backend.pictureUrl(screenBox.modelData.picture)
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                                sourceSize.width: 640
                            }

                            Rectangle {
                                anchors.fill: parent
                                visible: screenBox.hot
                                color: Qt.rgba(scroll.palette.highlight.r, scroll.palette.highlight.g, scroll.palette.highlight.b, 0.28)
                            }

                            Label {
                                anchors.centerIn: parent
                                visible: screenBox.hot
                                text: qsTr("Drop here")
                                color: "white"
                                font.pixelSize: 16
                            }

                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                height: 30
                                color: Qt.rgba(0, 0, 0, 0.55)

                                Label {
                                    id: screenLabel
                                    anchors.centerIn: parent
                                    width: parent.width - 12
                                    horizontalAlignment: Text.AlignHCenter
                                    color: "white"
                                    font.bold: true
                                    elide: Text.ElideRight
                                    text: screenBox.modelData.number + "  " + screenBox.modelData.model
                                }
                            }
                        }
                    }
                }

                DropArea {
                    anchors.fill: parent
                    onPositionChanged: (drag) => {
                        root.hover = root.screenAt(drag.x, drag.y, width, height)
                    }
                    onExited: root.hover = ""
                    onDropped: (drop) => {
                        const desc = root.screenAt(drop.x, drop.y, width, height)
                        root.hover = ""
                        if (root.dropPicture(root.droppedPath(drop), desc))
                            drop.accept(Qt.CopyAction)
                    }
                }
            }

            Label {
                text: qsTr("Pictures")
                font.bold: true
            }

            Label {
                visible: backend.pictures.length === 0
                text: qsTr("No pictures yet: choose one of your own.")
                opacity: 0.75
            }

            Flow {
                Layout.fillWidth: true
                spacing: 12

                Repeater {
                    model: backend.pictures

                    delegate: Item {
                        id: card
                        required property string modelData
                        objectName: "wallpaperPicture:" + modelData
                        readonly property string dragValue: modelData
                        width: 200
                        height: 112

                        Item {
                            id: ghost
                            width: card.width
                            height: card.height
                            Drag.active: cardArea.drag.active
                            Drag.source: card
                            Drag.hotSpot.x: width / 2
                            Drag.hotSpot.y: height / 2
                            z: 10

                            Rectangle {
                                anchors.fill: parent
                                radius: 8
                                clip: true
                                color: Qt.rgba(1, 1, 1, 0.08)
                                border.color: Qt.rgba(1, 1, 1, 0.3)

                                Image {
                                    anchors.fill: parent
                                    source: backend.pictureUrl(card.modelData)
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                    sourceSize.width: 400
                                }

                                Label {
                                    anchors.bottom: parent.bottom
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    width: parent.width - 8
                                    horizontalAlignment: Text.AlignHCenter
                                    elide: Text.ElideMiddle
                                    text: card.modelData.substring(card.modelData.lastIndexOf("/") + 1)
                                    font.pixelSize: 11
                                }
                            }
                        }

                        MouseArea {
                            id: cardArea
                            anchors.fill: parent
                            drag.target: ghost
                            onReleased: {
                                if (drag.active)
                                    ghost.Drag.drop()
                                ghost.x = 0
                                ghost.y = 0
                            }
                            onCanceled: {
                                ghost.x = 0
                                ghost.y = 0
                            }
                            onClicked: root.useEverywhere(card.modelData)
                        }
                    }
                }
            }

            Button {
                text: qsTr("Choose a Picture of Your Own…")
                onClicked: ownDialog.open()
            }
        }
    }

    FileDialog {
        id: ownDialog
        title: qsTr("Choose a wallpaper")
        nameFilters: [qsTr("Pictures (*.jpg *.jpeg *.png *.webp *.avif *.jxl)")]
        onAccepted: root.useEverywhere(backend.localPath(selectedFile))
    }
}
