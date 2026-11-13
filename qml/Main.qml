// The panel's window: a menu of the description's pages on the left, the page shown on the right.
// Everything drawn comes from the models (panel.pages); no option is named here.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window

    required property Controller panel
    property int current: 0
    readonly property PageItem page: panel.pages.length > current ? panel.pages[current] as PageItem : null

    width: 1100
    height: 780
    minimumWidth: 720
    minimumHeight: 480
    title: qsTr("Riley Settings")
    color: palette.window

    // The system-launcher page has no page of its own: its menu entry launches the system's settings.
    SystemLauncher {
        id: systemLauncher
        panel: window.panel
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 270
            color: window.palette.base

            ScrollView {
                id: menuScroll
                anchors.fill: parent
                contentWidth: availableWidth

                Column {
                    width: menuScroll.availableWidth
                    topPadding: 8
                    bottomPadding: 8

                    Repeater {
                        model: window.panel.pages

                        delegate: ItemDelegate {
                            required property PageItem modelData
                            required property int index

                            objectName: "page:" + modelData.pageId
                            width: parent ? parent.width : 0
                            text: modelData.title
                            highlighted: index === window.current
                            onClicked: {
                                if (modelData.widget === "system-launcher")
                                    systemLauncher.launch()   // the page shown stays on screen
                                else
                                    window.current = index
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 1
            color: window.palette.mid
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 20
            spacing: 10

            Label {
                Layout.fillWidth: true
                text: window.page ? window.page.title : ""
                font.pixelSize: 22
                font.bold: true
                elide: Text.ElideRight
            }

            Label {
                Layout.fillWidth: true
                visible: text !== ""
                text: window.page ? window.page.help : ""
                wrapMode: Text.WordWrap
                opacity: 0.75
            }

            Rectangle {
                Layout.fillWidth: true
                visible: window.panel.message !== ""
                implicitHeight: messageRow.implicitHeight + 12
                radius: 4
                color: Qt.rgba(window.palette.highlight.r, window.palette.highlight.g, window.palette.highlight.b, 0.18)
                border.color: window.palette.highlight

                RowLayout {
                    id: messageRow
                    anchors.fill: parent
                    anchors.margins: 6

                    Label {
                        Layout.fillWidth: true
                        text: window.panel.message
                        wrapMode: Text.WordWrap
                    }

                    Button {
                        text: qsTr("Dismiss")
                        flat: true
                        focusPolicy: Qt.NoFocus
                        onClicked: window.panel.clearMessage()
                    }
                }
            }

            // Every page is made once, at load, and kept: a page shown again is already laid out.
            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: window.current

                Repeater {
                    model: window.panel.pages

                    delegate: Loader {
                        id: pageLoader

                        required property PageItem modelData

                        // M3: a widget page is drawn by its own component (qml/pages/); a widget with no component
                        // here shows the placeholder, as in M2.
                        readonly property var widgetPages: ({
                            "displays": displaysPage,
                            "printers": printersPage,
                            "keyboard-mouse": keyboardMousePage,
                            "theme-wallpaper": themeWallpaperPage,
                            "dock": dockPage,
                            "desktop": desktopPage
                        })

                        sourceComponent: modelData.widget === "" ? drawnPage
                                                                 : (widgetPages[modelData.widget] ?? widgetPage)

                        Component { id: displaysPage; Displays { panel: window.panel; page: pageLoader.modelData } }
                        Component { id: printersPage; Printers { panel: window.panel; page: pageLoader.modelData } }
                        Component {
                            id: keyboardMousePage
                            KeyboardMouse { panel: window.panel; page: pageLoader.modelData }
                        }
                        Component {
                            id: themeWallpaperPage
                            ThemeWallpaper { panel: window.panel; page: pageLoader.modelData }
                        }
                        Component { id: dockPage; Dock { panel: window.panel; page: pageLoader.modelData } }
                        Component { id: desktopPage; Desktop { panel: window.panel; page: pageLoader.modelData } }

                        Component {
                            id: drawnPage

                            ScrollView {
                                id: pageScroll
                                contentWidth: availableWidth
                                clip: true

                                ColumnLayout {
                                    width: pageScroll.availableWidth
                                    spacing: 18

                                    Repeater {
                                        model: pageLoader.modelData.sections

                                        delegate: SectionBox {
                                            required property SectionItem modelData
                                            Layout.fillWidth: true
                                            section: modelData
                                        }
                                    }
                                }
                            }
                        }

                        // A widget page with no component here shows a placeholder naming that component.
                        Component {
                            id: widgetPage

                            Item {
                                Label {
                                    objectName: "placeholder:" + pageLoader.modelData.widget
                                    anchors.centerIn: parent
                                    width: Math.min(implicitWidth, parent.width)
                                    horizontalAlignment: Text.AlignHCenter
                                    wrapMode: Text.WordWrap
                                    text: qsTr("This page is drawn by its own component (%1), which is not here yet.")
                                        .arg(pageLoader.modelData.widget)
                                    opacity: 0.75
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
