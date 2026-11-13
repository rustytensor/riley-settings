// Keyboard and Mouse: the page's options are M2 controls (docs/pages-contract.md, "Keyboard and Mouse"), drawn in
// file order, with Riley's box to try key repeat (keyboard_mouse.py: "Try it here: hold a key down"), which saves
// nothing, and Riley's layout picker (docs/switch-contract.md, "Keyboard layout picker"): first layout, an optional
// second one and the key that switches, each layout with a variant, saved as one change. The root's objectName is
// widget:keyboard-mouse; the properties and functions below are the ones the gate uses (untyped parameters).
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

    readonly property list<string> layoutCodes: backend.layoutCodes
    readonly property list<string> switchKeys: backend.switchKeys

    function refresh() { return backend.refresh() }
    function variantsFor(code) { return backend.variantsFor(code) }
    function chooseLayouts(first, firstVariant, second, secondVariant, switchKey) {
        return backend.chooseLayouts(first, firstVariant, second, secondVariant, switchKey)
    }

    // The picker's controls call this with what they show (a layout picked anew starts without a variant), then
    // show the values again.
    function pickerChanged() {
        const first = backend.layoutCodes[firstLayout.currentIndex] ?? "us"
        const second = secondLayout.currentIndex > 0 ? (backend.layoutCodes[secondLayout.currentIndex - 1] ?? "") : ""
        const fv = firstVariant.variants[firstVariant.currentIndex] ?? ""
        const sv = second !== "" ? (secondVariant.variants[secondVariant.currentIndex] ?? "") : ""
        chooseLayouts(first, fv, second, sv, backend.switchKeys[switchKey.currentIndex] ?? "")
        firstLayout.currentIndex = Qt.binding(() => backend.layoutCodes.indexOf(backend.current.first ?? ""))
        secondLayout.currentIndex = Qt.binding(() => backend.layoutCodes.indexOf(backend.current.second ?? "") + 1)
        firstVariant.currentIndex = Qt.binding(() => Math.max(0, firstVariant.variants.indexOf(backend.current.firstVariant ?? "")))
        secondVariant.currentIndex = Qt.binding(() => Math.max(0, secondVariant.variants.indexOf(backend.current.secondVariant ?? "")))
        switchKey.currentIndex = Qt.binding(() => Math.max(0, backend.switchKeys.indexOf(backend.current.switchKey ?? "")))
    }

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

            // Riley's picker: "Layout", "Second layout" (None, or one to switch to) and "Switch layouts with".
            GridLayout {
                objectName: "keyboard-mouse:layouts"
                Layout.fillWidth: true
                columns: 3
                columnSpacing: 12
                rowSpacing: 8

                Label { text: qsTr("Layout") }
                ComboBox {
                    id: firstLayout
                    objectName: "keyboard-mouse:first-layout"
                    Layout.fillWidth: true
                    model: backend.layoutNames
                    currentIndex: backend.layoutCodes.indexOf(backend.current.first ?? "")
                    onActivated: { firstVariant.currentIndex = 0; root.pickerChanged() }
                }
                ComboBox {
                    id: firstVariant
                    objectName: "keyboard-mouse:first-variant"
                    readonly property var variants: backend.variantsFor(backend.layoutCodes[firstLayout.currentIndex] ?? "")
                    model: variants.map(v => v === "" ? qsTr("Standard") : v)
                    currentIndex: Math.max(0, variants.indexOf(backend.current.firstVariant ?? ""))
                    onActivated: root.pickerChanged()
                }

                Label { text: qsTr("Second layout") }
                ComboBox {
                    id: secondLayout
                    objectName: "keyboard-mouse:second-layout"
                    Layout.fillWidth: true
                    model: [qsTr("None")].concat(backend.layoutNames)
                    currentIndex: backend.layoutCodes.indexOf(backend.current.second ?? "") + 1
                    onActivated: { secondVariant.currentIndex = 0; root.pickerChanged() }
                }
                ComboBox {
                    id: secondVariant
                    objectName: "keyboard-mouse:second-variant"
                    enabled: secondLayout.currentIndex > 0
                    readonly property var variants: secondLayout.currentIndex > 0
                        ? backend.variantsFor(backend.layoutCodes[secondLayout.currentIndex - 1] ?? "") : [""]
                    model: variants.map(v => v === "" ? qsTr("Standard") : v)
                    currentIndex: Math.max(0, variants.indexOf(backend.current.secondVariant ?? ""))
                    onActivated: root.pickerChanged()
                }

                Label { text: qsTr("Switch layouts with") }
                ComboBox {
                    id: switchKey
                    objectName: "keyboard-mouse:switch-key"
                    Layout.fillWidth: true
                    enabled: secondLayout.currentIndex > 0
                    model: backend.switchKeyNames
                    currentIndex: Math.max(0, backend.switchKeys.indexOf(backend.current.switchKey ?? ""))
                    onActivated: root.pickerChanged()
                }
                Item { Layout.preferredWidth: 1 }
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
