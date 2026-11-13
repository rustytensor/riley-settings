// type="number": a decimal number, typed (committed on Return or when the field loses focus) or stepped with the
// buttons beside it.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

FocusScope {
    id: control

    required property OptionItem option
    readonly property string controlType: option.type
    readonly property string valueText: option.valueText

    function sync() {
        field.text = valueText;
    }

    function commitValue(value) {
        const ok = option.commitNumber(Number(value));
        sync();
        return ok;
    }

    function stepBy(dir) {
        let v = Number(valueText) + dir * option.step;
        if (option.hasMin)
            v = Math.max(v, option.min);
        if (option.hasMax)
            v = Math.min(v, option.max);
        commitValue(v);
    }

    implicitWidth: layout.implicitWidth
    implicitHeight: layout.implicitHeight
    Component.onCompleted: sync()
    onValueTextChanged: sync()

    RowLayout {
        id: layout
        anchors.fill: parent
        spacing: 4

        Button {
            text: "−"
            focusPolicy: Qt.NoFocus
            implicitWidth: 36
            onClicked: control.stepBy(-1)
        }

        TextField {
            id: field
            focus: true
            Layout.preferredWidth: 140
            horizontalAlignment: Text.AlignRight
            font.family: "monospace"
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            validator: RegularExpressionValidator { regularExpression: /[+-]?(\d+(\.\d*)?|\.\d+)/ }
            onEditingFinished: if (text !== control.valueText) control.commitValue(text)
            onActiveFocusChanged: if (!activeFocus) control.sync()
        }

        Button {
            text: "+"
            focusPolicy: Qt.NoFocus
            implicitWidth: 36
            onClicked: control.stepBy(1)
        }
    }
}
