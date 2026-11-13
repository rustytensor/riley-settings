// type="keybinding": click, then press the keys. The binding is the modifiers in the order SUPER, CTRL, ALT, SHIFT,
// then the key, joined by "+". Escape cancels. Super pressed and released with no other key is "SUPER+SUPER_L" (or
// SUPER_R), which Hyprland takes as a binding on the release of Super alone.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

FocusScope {
    id: control

    required property OptionItem option
    readonly property string controlType: option.type
    readonly property string valueText: option.valueText
    property bool capturing: false
    property int aloneKey: 0   // the Super key held with nothing else pressed since; 0 otherwise

    function commitValue(value) {
        capturing = false;
        aloneKey = 0;
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
                control.aloneKey = 0;
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
                const others = event.modifiers & ~Qt.MetaModifier;
                control.aloneKey = (others === Qt.NoModifier && control.option.aloneText(event.key) !== "") ? event.key : 0;
                const text = control.option.bindingText(event.key, event.modifiers);
                if (text !== "")
                    control.commitValue(text);
            }
            Keys.onReleased: event => {
                if (!control.capturing)
                    return;
                event.accepted = true;
                if (event.key === control.aloneKey && !event.isAutoRepeat)
                    control.commitValue(control.option.aloneText(event.key));
            }
        }

        Button {
            visible: control.valueText !== control.option.defaultText
            text: qsTr("Default")
            onClicked: control.commitValue(control.option.defaultText)
        }
    }
}
