// Native portal content, also testable without the surrounding Kirigami window.
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts

ColumnLayout {
    id: page
    required property var model
    property var rectangle: model.rectangle
    property var portal: model.portal
    property var prefs: model.preferences
    spacing: 12

    Controls.Switch {
        id: portalSwitch
        objectName: "cameraPassthroughEnabled"
        text: "Passthrough"
        Binding { target: portalSwitch; property: "checked"; value: page.portal.requested === true }
        onToggled: page.model.setWanted(checked)
    }
    Controls.Label {
        objectName: "cameraPassthroughStatus"
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        textFormat: Text.PlainText
        text: page.portal.summary
    }
    Controls.Switch {
        id: controlsSwitch
        objectName: "passthroughControls"
        text: "Show placement controls"
        Binding {
            target: controlsSwitch; property: "checked"
            value: page.prefs.controls.known ? page.prefs.controls.enabled : page.prefs.controls.saved
        }
        onToggled: page.model.setControls(checked)
    }
    Controls.Label {
        objectName: "passthroughControlsStatus"
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        textFormat: Text.PlainText
        text: page.prefs.controls.summary
    }
    Controls.Switch {
        id: layerCheck
        objectName: "portalAboveSteam"
        text: "Passthrough above Steam menus"
        Binding {
            target: layerCheck; property: "checked"
            value: page.prefs.layer.known ? page.prefs.layer.enabled : page.prefs.layer.saved
        }
        onToggled: page.model.setAboveSteam(checked)
    }
    Controls.Label {
        objectName: "portalLayerStatus"
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        textFormat: Text.PlainText
        text: page.prefs.layer.summary
    }
    Controls.Label {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        text: "Move and rotate the rectangle with its grab bar. Drag an edge handle to resize it. "
            + "Showing placement controls does not turn passthrough on."
    }
    Controls.Button {
        objectName: "passthroughPlace"
        text: "Place at desk"
        onClicked: page.model.placeAtDesk()
    }
    Controls.Label {
        objectName: "passthroughGeometryStatus"
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        textFormat: Text.PlainText
        text: page.model.geometrySummary
    }
    GridLayout {
        columns: 2
        Layout.fillWidth: true
        Repeater {
            model: [
                { name: "Width", label: "Width (m)", key: "size", axis: 0, low: 0.05, high: 5, decimals: 3 },
                { name: "Depth", label: "Length (m)", key: "size", axis: 2, low: 0.05, high: 5, decimals: 3 },
                { name: "X", label: "Center X, right (m)", key: "center", axis: 0, low: -100, high: 100, decimals: 3 },
                { name: "Y", label: "Center Y, height (m)", key: "center", axis: 1, low: -100, high: 100, decimals: 3 },
                { name: "Z", label: "Center Z, forward is negative (m)", key: "center", axis: 2, low: -100, high: 100, decimals: 3 },
                { name: "Yaw", label: "Yaw (degrees)", key: "yaw", axis: -1, low: -180, high: 180, decimals: 1 },
                { name: "Pitch", label: "Pitch (degrees)", key: "pitch", axis: -1, low: -180, high: 180, decimals: 1 },
                { name: "Roll", label: "Roll (degrees)", key: "roll", axis: -1, low: -180, high: 180, decimals: 1 }
            ]
            delegate: RowLayout {
                required property var modelData
                Layout.fillWidth: true
                Controls.Label { text: modelData.label; Layout.fillWidth: true }
                Controls.TextField {
                    id: field
                    property bool edited: false
                    property string current: Number(modelData.axis < 0 ? (page.rectangle[modelData.key] || 0)
                        : page.rectangle[modelData.key][modelData.axis]).toFixed(modelData.decimals)
                    objectName: "passthrough" + modelData.name
                    Accessible.name: modelData.label
                    Layout.preferredWidth: 95
                    Binding {
                        target: field; property: "text"; value: field.current
                        when: !field.activeFocus && !field.edited
                        restoreMode: Binding.RestoreNone
                    }
                    validator: DoubleValidator {
                        bottom: modelData.low; top: modelData.high; decimals: modelData.decimals
                        notation: DoubleValidator.StandardNotation; locale: "C"
                    }
                    onTextEdited: edited = true
                    onActiveFocusChanged: {
                        if (!activeFocus && !acceptableInput) { text = current; edited = false }
                    }
                    onEditingFinished: {
                        if (edited && acceptableInput)
                            page.model.setGeometry(modelData.key, modelData.axis, parseFloat(text))
                        edited = false
                    }
                }
            }
        }
    }
    Controls.Label {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        text: "Place at desk preserves the rectangle's size and tilt. Placement never turns passthrough on."
    }
}
