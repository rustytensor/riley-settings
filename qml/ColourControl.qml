// type="colour": a swatch and its #rrggbb (or #rrggbbaa) text; the colour dialog commits on OK.
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

FocusScope {
    id: control

    required property OptionItem option
    readonly property string controlType: option.type
    readonly property string valueText: option.valueText
    // QML reads #aarrggbb; the values file writes #rrggbbaa.
    readonly property color shown: valueText.length === 9 ? "#" + valueText.slice(7, 9) + valueText.slice(1, 7)
                                                          : (valueText !== "" ? valueText : "transparent")

    function hex(c) {
        const two = v => ("0" + Math.round(v * 255).toString(16)).slice(-2);
        let s = "#" + two(c.r) + two(c.g) + two(c.b);
        if (c.a < 1)
            s += two(c.a);
        return s;
    }

    function commitValue(value) {
        return option.commit(String(value));
    }

    implicitWidth: layout.implicitWidth
    implicitHeight: layout.implicitHeight

    RowLayout {
        id: layout
        anchors.fill: parent

        Rectangle {
            implicitWidth: 40
            implicitHeight: 26
            radius: 3
            color: control.shown
            border.color: control.palette.mid

            MouseArea {
                anchors.fill: parent
                onClicked: dialog.open()
            }
        }

        Label {
            Layout.preferredWidth: 90
            text: control.valueText
            font.family: "monospace"
        }

        Button {
            focus: true
            text: qsTr("Choose…")
            onClicked: dialog.open()
        }
    }

    ColorDialog {
        id: dialog
        title: control.option.label
        selectedColor: control.shown
        options: ColorDialog.ShowAlphaChannel
        onAccepted: control.commitValue(control.hex(selectedColor))
    }
}
