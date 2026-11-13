// type="font": the family and the size ("Noto Sans 12"), shown in that font; the font dialog commits on OK.
// Empty means the theme's own font.
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

FocusScope {
    id: control

    required property OptionItem option
    readonly property string controlType: option.type
    readonly property string valueText: option.valueText
    readonly property var parts: {
        const m = valueText.match(/^(.*\S)\s+(\d+(\.\d+)?)$/);
        return m ? {family: m[1], size: Number(m[2])} : {family: valueText, size: 0};
    }

    function commitValue(value) {
        return option.commit(String(value));
    }

    implicitWidth: layout.implicitWidth
    implicitHeight: layout.implicitHeight

    RowLayout {
        id: layout
        anchors.fill: parent

        Label {
            Layout.preferredWidth: 180
            text: control.parts.family !== "" ? control.parts.family : qsTr("Theme font")
            font.family: control.parts.family !== "" ? control.parts.family : Qt.application.font.family
            elide: Text.ElideRight
        }

        Label {
            Layout.preferredWidth: 32
            text: control.parts.size > 0 ? String(control.parts.size) : ""
            font.family: "monospace"
        }

        Button {
            focus: true
            text: qsTr("Choose…")
            onClicked: dialog.open()
        }

        Button {
            visible: control.valueText !== control.option.defaultText
            text: qsTr("Default")
            onClicked: control.commitValue(control.option.defaultText)
        }
    }

    FontDialog {
        id: dialog
        title: control.option.label
        // A FocusScope has no font of its own: the application's font stands for the theme's.
        selectedFont: Qt.font({family: control.parts.family || Qt.application.font.family,
                               pointSize: control.parts.size > 0 ? control.parts.size : Qt.application.font.pointSize})
        onAccepted: control.commitValue(selectedFont.family + " " + Math.round(selectedFont.pointSize))
    }
}
