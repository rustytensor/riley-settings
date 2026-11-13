// What type="file" and type="folder" share: the path, typed (committed on Return or when the field loses focus),
// or picked in the file or folder dialog, which commits on OK.
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

FocusScope {
    id: control

    required property OptionItem option
    property bool folderMode: false
    readonly property string controlType: option.type
    readonly property string valueText: option.valueText

    function sync() {
        field.text = valueText;
    }

    function commitValue(value) {
        const ok = option.commit(String(value));
        sync();
        return ok;
    }

    function choose() {
        if (folderMode) {
            folderDialog.currentFolder = option.urlOf(valueText);
            folderDialog.open();
        } else {
            fileDialog.selectedFile = option.urlOf(valueText);
            fileDialog.open();
        }
    }

    implicitWidth: layout.implicitWidth
    implicitHeight: layout.implicitHeight
    Component.onCompleted: sync()
    onValueTextChanged: sync()

    RowLayout {
        id: layout
        anchors.fill: parent

        TextField {
            id: field
            focus: true
            Layout.preferredWidth: 380
            placeholderText: qsTr("None")
            onEditingFinished: if (text !== control.valueText) control.commitValue(text)
            onActiveFocusChanged: if (!activeFocus) control.sync()
        }

        Button {
            text: qsTr("Choose…")
            onClicked: control.choose()
        }
    }

    FileDialog {
        id: fileDialog
        title: control.option.label
        onAccepted: control.commitValue(control.option.localPath(selectedFile))
    }

    FolderDialog {
        id: folderDialog
        title: control.option.label
        onAccepted: control.commitValue(control.option.localPath(selectedFolder))
    }
}
