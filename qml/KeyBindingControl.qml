// type="keybinding": click, then press the keys. The binding is the modifiers in the order SUPER, CTRL, ALT, SHIFT,
// then the key, joined by "+". Escape cancels.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

FocusScope {
    id: control

    required property OptionItem option
    readonly property string controlType: option.type
    readonly property string valueText: option.valueText
    property bool capturing: false

    function commitValue(value) {
        capturing = false;
        return option.commit(String(value));
    }

    implicitWidth: layout.implicitWidth
    implicitHeight: layout.implicitHeight

    RowLayout {
        id: layout
        anchors.fill: parent

        Button {
            id: capture
            focus: true
            Layout.preferredWidth: 240
            checkable: false
            highlighted: control.capturing
            text: control.capturing ? qsTr("Press the keys…") : (control.valueText !== "" ? control.valueText : qsTr("None"))
            font.family: control.capturing ? control.font.family : "monospace"
            onClicked: {
                control.capturing = true;
                forceActiveFocus();
            }
            onActiveFocusChanged: if (!activeFocus) control.capturing = false
            Keys.onPressed: event => {
                if (!control.capturing)
                    return;
                event.accepted = true;
                if (event.key === Qt.Key_Escape && event.modifiers === Qt.NoModifier) {
                    control.capturing = false;
                    return;
                }
                const text = control.option.bindingText(event.key, event.modifiers);
                if (text !== "")
                    control.commitValue(text);
            }
        }

        Button {
            visible: control.valueText !== control.option.defaultText
            text: qsTr("Default")
            onClicked: control.commitValue(control.option.defaultText)
        }
    }
}
