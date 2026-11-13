// type="text": one line of text, committed on Return or when the field loses focus.
import QtQuick
import QtQuick.Controls

TextField {
    id: control

    required property OptionItem option
    readonly property string controlType: option.type
    readonly property string valueText: option.valueText

    function sync() {
        text = valueText;
    }

    function commitValue(value) {
        const ok = option.commit(String(value));
        sync();
        return ok;
    }

    implicitWidth: 320
    placeholderText: option.defaultText === "" ? qsTr("Empty") : option.defaultText
    Component.onCompleted: sync()
    onValueTextChanged: sync()
    onEditingFinished: if (text !== valueText) commitValue(text)
}
