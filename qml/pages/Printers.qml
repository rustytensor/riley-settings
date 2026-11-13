// Printers: CUPS queues, jobs, your options and default, through the CUPS command-line tools (docs/pages-contract.md,
// "Printers"; a port of Riley's printers.py). The root's objectName is widget:printers. The functions below are the
// ones the gate calls (untyped parameters); the page's buttons and boxes call the same ones.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    objectName: "widget:printers"

    required property Controller panel
    required property PageItem page

    readonly property var printers: backend.printers
    readonly property var jobs: backend.jobs
    readonly property var stateText: ({ "idle": qsTr("Ready"), "printing": qsTr("Printing"), "stopped": qsTr("Stopped") })

    PrintersBackend {
        id: backend
        panel: root.panel
        page: root.page
    }

    function refresh() { return backend.refresh() }
    function printTestPage(queue) { return backend.printTestPage(queue) }
    function cancelJob(requestId) { return backend.cancelJob(requestId) }
    function setOption(queue, option, value) { return backend.setOption(queue, option, value) }
    function userOption(queue, option) { return backend.userOption(queue, option) }
    function setDefault(queue) { return backend.setDefault(queue) }
    function openSetupTool() { return backend.openSetupTool() }
    function optionNames(queue) { return backend.optionNames(queue) }
    function optionChoices(queue, option) { return backend.optionChoices(queue, option) }
    function optionLabels(queue, option) { return backend.optionLabels(queue, option) }
    function optionValue(queue, option) { return backend.optionValue(queue, option) }

    // Read when the page is shown, then every 3 s while it stays shown (Riley's REFRESH_SECONDS).
    onVisibleChanged: if (visible) refresh()
    Component.onCompleted: if (visible) refresh()

    Timer {
        interval: 3000
        repeat: true
        running: root.visible
        onTriggered: backend.refresh()
    }

    ScrollView {
        id: scroll
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: scroll.availableWidth
            spacing: 18

            RowLayout {
                Layout.fillWidth: true

                Label {
                    Layout.fillWidth: true
                    visible: !backend.available || root.printers.length === 0
                    text: !backend.available ? backend.problem
                                             : qsTr("No printers. Add one with Add Printer…; network printers are usually found by themselves.")
                    wrapMode: Text.WordWrap
                    opacity: 0.75
                }

                Item { Layout.fillWidth: true; visible: backend.available && root.printers.length > 0 }

                Button {
                    objectName: "printers:add"
                    text: qsTr("Add Printer…")
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Fedora's printer setup (asks for your password)")
                    onClicked: root.openSetupTool()
                }
            }

            Repeater {
                model: root.printers

                delegate: Frame {
                    id: card

                    required property var modelData
                    readonly property string queue: modelData.name

                    objectName: "printer:" + queue
                    property string printerInfo: modelData.info
                    property string printerLocation: modelData.location
                    property string printerState: modelData.state
                    property bool accepting: modelData.accepting
                    property bool isDefault: modelData.isDefault

                    Layout.fillWidth: true

                    ColumnLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        spacing: 8

                        RowLayout {
                            Layout.fillWidth: true

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Label {
                                    Layout.fillWidth: true
                                    text: card.printerInfo
                                    font.bold: true
                                    elide: Text.ElideRight
                                }

                                Label {
                                    Layout.fillWidth: true
                                    visible: text !== ""
                                    text: [card.modelData.model, card.printerLocation].filter(x => x).join(" · ")
                                    opacity: 0.7
                                    elide: Text.ElideRight
                                }
                            }

                            Label {
                                visible: card.isDefault
                                text: qsTr("Default printer")
                                color: card.palette.highlight
                            }

                            Button {
                                visible: !card.isDefault
                                text: qsTr("Make Default")
                                flat: true
                                onClicked: root.setDefault(card.queue)
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: qsTr("Status: ") + (root.stateText[card.printerState] ?? card.printerState)
                                  + (card.modelData.reasons.length > 0 ? " — " + card.modelData.reasons.join(", ").replace(/-/g, " ") : "")
                                  + (card.accepting ? "" : qsTr(" (not accepting jobs)"))
                        }

                        // Riley's option rows: paper size, colour, quality, two-sided, each when the printer offers
                        // more than one choice; the one shown is yours, else the printer's default.
                        Repeater {
                            model: card.modelData.options

                            delegate: RowLayout {
                                id: optionRow

                                required property var modelData

                                Layout.fillWidth: true

                                Label {
                                    Layout.fillWidth: true
                                    text: optionRow.modelData.title
                                }

                                ComboBox {
                                    objectName: "printer-option:" + card.queue + ":" + optionRow.modelData.name
                                    property string option: optionRow.modelData.name
                                    property string value: optionRow.modelData.value
                                    model: optionRow.modelData.labels
                                    currentIndex: Math.max(0, optionRow.modelData.choices.indexOf(optionRow.modelData.value))
                                    onActivated: index => {
                                        root.setOption(card.queue, optionRow.modelData.name, optionRow.modelData.choices[index])
                                        currentIndex = Qt.binding(() => Math.max(0, optionRow.modelData.choices.indexOf(optionRow.modelData.value)))
                                    }
                                }
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Print queue")
                            font.bold: true
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: queueJobs.count === 0
                            text: qsTr("Nothing waiting")
                            opacity: 0.7
                        }

                        Repeater {
                            id: queueJobs
                            model: root.jobs.filter(j => j.queue === card.queue)

                            delegate: RowLayout {
                                id: jobRow

                                required property var modelData

                                objectName: "job:" + modelData.id
                                property string jobState: modelData.state
                                Layout.fillWidth: true

                                Label {
                                    Layout.fillWidth: true
                                    text: jobRow.modelData.id + " · " + jobRow.jobState + " · " + jobRow.modelData.user
                                    elide: Text.ElideRight
                                }

                                Button {
                                    text: qsTr("Cancel")
                                    flat: true
                                    onClicked: root.cancelJob(jobRow.modelData.id)
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Test page: prints one page so you can check the printer works")
                                wrapMode: Text.WordWrap
                                opacity: 0.8
                            }

                            Button {
                                text: qsTr("Print Test Page")
                                onClicked: root.printTestPage(card.queue)
                            }

                            Button {
                                text: qsTr("Printer Setup…")
                                flat: true
                                ToolTip.visible: hovered
                                ToolTip.text: qsTr("Rename, remove, sharing, drivers (Fedora's printer tool)")
                                onClicked: root.openSetupTool()
                            }
                        }
                    }
                }
            }
        }
    }
}
