// type="list": one row per item with Edit and Remove, and Add. Edit and Add open the item editor, whose OK commits
// the whole list (commitItems), as Remove does.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: control

    required property OptionItem option
    readonly property string controlType: option.type
    readonly property int itemCount: option.items.length

    function commitItems(items) {
        const list = [];
        for (let i = 0; i < items.length; ++i) {
            const one = {};
            for (const name in items[i])
                one[name] = String(items[i][name]);
            list.push(one);
        }
        return option.commitItems(list);
    }

    function copyItems() {
        const out = [];
        for (let i = 0; i < option.items.length; ++i)
            out.push(Object.assign({}, option.items[i]));
        return out;
    }

    function shown(field, value) {
        if (field.type === "switch")
            return value === "true" ? qsTr("yes") : qsTr("no");
        for (let i = 0; i < field.choices.length; ++i) {
            if (field.choices[i].value === value)
                return field.choices[i].label;
        }
        return value;
    }

    spacing: 4

    Repeater {
        model: control.option.items

        delegate: Frame {
            id: itemRow

            required property var modelData
            required property int index

            Layout.fillWidth: true
            padding: 6

            RowLayout {
                anchors.left: parent.left
                anchors.right: parent.right

                Flow {
                    Layout.fillWidth: true
                    spacing: 14

                    Repeater {
                        model: control.option.fields

                        delegate: Label {
                            required property var modelData
                            text: modelData.label + ": " + control.shown(modelData, itemRow.modelData[modelData.name])
                        }
                    }
                }

                Button {
                    text: qsTr("Edit")
                    onClicked: editor.openFor(itemRow.index)
                }

                Button {
                    text: qsTr("Remove")
                    onClicked: {
                        const items = control.copyItems();
                        items.splice(itemRow.index, 1);
                        control.commitItems(items);
                    }
                }
            }
        }
    }

    Label {
        visible: control.itemCount === 0
        text: qsTr("None yet")
        opacity: 0.7
    }

    Button {
        text: qsTr("Add")
        onClicked: editor.openFor(-1)
    }

    Dialog {
        id: editor

        property int editing: -1
        property var draft: ({})

        function openFor(index) {
            editing = index;
            if (index >= 0) {
                draft = Object.assign({}, control.option.items[index]);
            } else {
                const d = {};
                for (let i = 0; i < control.option.fields.length; ++i)
                    d[control.option.fields[i].name] = control.option.fields[i].defaultText;
                draft = d;
            }
            open();
        }

        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        width: Math.min(560, parent ? parent.width - 40 : 560)
        title: control.option.label
        standardButtons: Dialog.Ok | Dialog.Cancel

        onAccepted: {
            const items = control.copyItems();
            if (editing >= 0)
                items[editing] = draft;
            else
                items.push(draft);
            control.commitItems(items);
        }

        contentItem: Loader {
            active: editor.visible

            sourceComponent: ColumnLayout {
                spacing: 8

                Repeater {
                    model: control.option.fields

                    delegate: RowLayout {
                        id: fieldRow

                        required property var modelData

                        Layout.fillWidth: true

                        Label {
                            Layout.preferredWidth: 180
                            text: fieldRow.modelData.label
                            wrapMode: Text.WordWrap
                        }

                        Loader {
                            Layout.fillWidth: true
                            sourceComponent: fieldRow.modelData.type === "switch" ? checkEditor
                                           : fieldRow.modelData.type === "choice" ? choiceEditor : textEditor
                        }

                        Component {
                            id: checkEditor

                            CheckBox {
                                checked: editor.draft[fieldRow.modelData.name] === "true"
                                onToggled: editor.draft[fieldRow.modelData.name] = checked ? "true" : "false"
                            }
                        }

                        Component {
                            id: choiceEditor

                            ComboBox {
                                model: fieldRow.modelData.choices
                                textRole: "label"
                                valueRole: "value"
                                Component.onCompleted: currentIndex = indexOfValue(editor.draft[fieldRow.modelData.name])
                                onActivated: index => editor.draft[fieldRow.modelData.name] = valueAt(index)
                            }
                        }

                        Component {
                            id: textEditor

                            TextField {
                                text: editor.draft[fieldRow.modelData.name] ?? ""
                                onTextEdited: editor.draft[fieldRow.modelData.name] = text
                            }
                        }
                    }
                }
            }
        }
    }
}
