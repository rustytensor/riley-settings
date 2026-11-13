// type="choice": one of the option's choices, shown by label, saved by value. Up and Down with the list closed move
// one choice and commit at once.
import QtQuick
import QtQuick.Controls

ComboBox {
    id: control

    required property OptionItem option
    readonly property string controlType: option.type
    readonly property string valueText: option.valueText

    function sync() {
        currentIndex = indexOfValue(valueText);
    }

    function commitValue(value) {
        const ok = option.commit(String(value));
        sync();
        return ok;
    }

    implicitWidth: 280
    model: option.choices
    textRole: "label"
    valueRole: "value"
    Component.onCompleted: sync()
    onValueTextChanged: sync()
    onActivated: index => commitValue(valueAt(index))
}
