// type="switch": on or off; a toggle is one change.
import QtQuick
import QtQuick.Controls

Switch {
    id: control

    required property OptionItem option
    readonly property string controlType: option.type
    readonly property string valueText: option.valueText

    function commitValue(value) {
        const ok = option.commit(String(value));
        checked = Qt.binding(() => control.valueText === "true");
        return ok;
    }

    checked: valueText === "true"
    onToggled: commitValue(checked ? "true" : "false")
}
