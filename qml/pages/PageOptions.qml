// The scalar options of a widget page, each drawn as its M2 control (OptionRow: objectName option:<key>), in file
// order (docs/pages-contract.md, "Scalar options of a widget page are M2 controls"). Shared by the keyboard-mouse,
// dock and desktop pages: change it only by agreement.
import QtQuick
import QtQuick.Layouts

ColumnLayout {
    id: box

    required property PageItem page

    spacing: 12

    Repeater {
        model: box.page ? box.page.scalarOptions : []

        delegate: OptionRow {
            required property OptionItem modelData
            Layout.fillWidth: true
            option: modelData
        }
    }
}
