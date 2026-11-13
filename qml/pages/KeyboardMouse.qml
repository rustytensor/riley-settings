// Keyboard and Mouse: the page's options are M2 controls (docs/pages-contract.md, "Keyboard and Mouse"), drawn in
// file order, with Riley's box to try key repeat (keyboard_mouse.py: "Try it here: hold a key down"), which saves
// nothing. The root's objectName is widget:keyboard-mouse.
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

    ScrollView {
        id: scroll
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: scroll.availableWidth - 32
            x: 16
            spacing: 12

            Item { Layout.preferredHeight: 4 }

            Label {
                Layout.fillWidth: true
                text: root.page ? root.page.title : ""
                font.bold: true
                font.pixelSize: 16
            }

            Label {
                Layout.fillWidth: true
                visible: text !== ""
                text: root.page ? root.page.help : ""
                wrapMode: Text.WordWrap
                opacity: 0.7
                font.pixelSize: 12
            }

            Repeater {
                model: backend.options

                delegate: OptionRow {
                    required property OptionItem modelData
                    Layout.fillWidth: true
                    option: modelData
                }
            }

            // Riley's try box: hold a key down to feel the repeat delay and speed. Saves nothing.
            TextField {
                objectName: "keyboard-mouse:try"
                Layout.fillWidth: true
                placeholderText: qsTr("Try it here: hold a key down")
            }

            Item { Layout.preferredHeight: 16 }
        }
    }
}
