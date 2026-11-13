// One section of a drawn page: its title, its help line, and one row per option, in file order.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: box

    required property SectionItem section

    spacing: 6

    Label {
        Layout.fillWidth: true
        text: box.section.title
        font.bold: true
        font.pixelSize: 16
    }

    Label {
        Layout.fillWidth: true
        visible: text !== ""
        text: box.section.help
        wrapMode: Text.WordWrap
        opacity: 0.75
    }

    Frame {
        Layout.fillWidth: true

        ColumnLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: 12

            Repeater {
                model: box.section.options

                delegate: OptionRow {
                    required property OptionItem modelData
                    Layout.fillWidth: true
                    option: modelData
                }
            }
        }
    }
}
