// One option: its label and help, and its control, chosen by the option's type (one component per type).
// A list takes the full width below its label; every other control sits on the right.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

GridLayout {
    id: row

    required property OptionItem option
    readonly property bool wide: option.type === "list"
    readonly property var controls: ({
        "switch": switchControl,
        "choice": choiceControl,
        "slider": sliderControl,
        "number": numberControl,
        "text": textControl,
        "colour": colourControl,
        "font": fontControl,
        "file": fileControl,
        "folder": folderControl,
        "keybinding": keyBindingControl,
        "list": listControl
    })

    columns: wide ? 1 : 2
    columnSpacing: 16
    rowSpacing: 6

    ColumnLayout {
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignVCenter
        spacing: 2

        Label {
            Layout.fillWidth: true
            text: row.option.label
            wrapMode: Text.WordWrap
        }

        Label {
            Layout.fillWidth: true
            visible: text !== ""
            text: row.option.help
            wrapMode: Text.WordWrap
            opacity: 0.7
            font.pixelSize: 12
        }
    }

    Loader {
        Layout.fillWidth: row.wide
        Layout.alignment: row.wide ? Qt.AlignLeft : (Qt.AlignRight | Qt.AlignVCenter)
        sourceComponent: row.controls[row.option.type] ?? unknownControl
        onLoaded: item.objectName = "option:" + row.option.key
    }

    Component { id: switchControl; SwitchControl { option: row.option } }
    Component { id: choiceControl; ChoiceControl { option: row.option } }
    Component { id: sliderControl; SliderControl { option: row.option } }
    Component { id: numberControl; NumberControl { option: row.option } }
    Component { id: textControl; TextControl { option: row.option } }
    Component { id: colourControl; ColourControl { option: row.option } }
    Component { id: fontControl; FontControl { option: row.option } }
    Component { id: fileControl; FileControl { option: row.option } }
    Component { id: folderControl; FolderControl { option: row.option } }
    Component { id: keyBindingControl; KeyBindingControl { option: row.option } }
    Component { id: listControl; ListControl { option: row.option } }
    Component { id: unknownControl; Label { text: qsTr("(no control for type %1)").arg(row.option.type) } }
}
