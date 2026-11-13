// type="slider": min to max in steps, with its value beside it. A key press moves one step and commits; a drag
// commits when released.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

FocusScope {
    id: control

    required property OptionItem option
    readonly property string controlType: option.type
    readonly property string valueText: option.valueText

    function sync() {
        slider.value = Number(valueText);
    }

    function commitValue(value) {
        const ok = option.commitNumber(Number(value));
        sync();
        return ok;
    }

    implicitWidth: layout.implicitWidth
    implicitHeight: layout.implicitHeight
    Component.onCompleted: sync()
    onValueTextChanged: sync()

    RowLayout {
        id: layout
        anchors.fill: parent

        Slider {
            id: slider
            focus: true
            Layout.preferredWidth: 240
            from: control.option.min
            to: control.option.max
            stepSize: control.option.step
            snapMode: Slider.SnapAlways
            onPressedChanged: if (!pressed) control.commitValue(value)
            onMoved: if (!pressed) control.commitValue(value)
            Keys.onPressed: event => {
                let dir = 0;
                if (event.key === Qt.Key_Right || event.key === Qt.Key_Up)
                    dir = 1;
                else if (event.key === Qt.Key_Left || event.key === Qt.Key_Down)
                    dir = -1;
                if (dir === 0)
                    return;
                event.accepted = true;
                if (dir > 0)
                    increase();
                else
                    decrease();
                control.commitValue(value);
            }
        }

        Label {
            Layout.preferredWidth: 52
            horizontalAlignment: Text.AlignRight
            text: control.valueText
            font.family: "monospace"
        }
    }
}
