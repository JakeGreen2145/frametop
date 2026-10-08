// Native portal content, also testable without the surrounding Kirigami window.
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts

ColumnLayout {
    id: page
    required property var model
    property var volume: model.volume
    property var portal: model.portal
    property var prefs: model.preferences
    spacing: 12

    Controls.Switch {
        id: portalSwitch
        objectName: "cameraPassthroughEnabled"
        text: page.portal.busy ? "Portal request pending" : page.portal.known
              ? (page.portal.wanted ? "Portal requested on" : "Portal off") : "Portal state unknown"
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
    Controls.Button {
        text: "Request portal Off"
        // Always available, including when an unavailable backend left intent unknown.
        onClicked: page.model.setWanted(false)
    }
    Controls.Label {
        objectName: "nativePortalCaveat"
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        text: "On and Off control the native portal request in ft-screens. Opening or closing settings does not change it. "
            + "Off releases only this portal's request; Valve may still own full Room View and camera hardware. "
            + "Active/submitting means a native frame was submitted, not proof of visible pixels or a fresh camera exposure. "
            + "Camera freshness is unknown. This experimental view is not a safety boundary."
    }
    Controls.Switch {
        id: controlsSwitch
        objectName: "passthroughControls"
        text: "Show controls"
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
    Controls.CheckBox {
        id: layerCheck
        objectName: "portalAboveSteam"
        text: "Keep portal controls visible with Steam menus"
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
        text: page.prefs.layer.summary + ". Default: hide controls with Steam menus. "
            + "This preference does not prove native camera composition above Steam. The native backend reports menu suppression separately."
    }
    Controls.Label {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        text: "One room-fixed box defines the portal's top face, independently of screen layouts and profiles. "
            + "Show controls reveals its edges, resize handles and grab bar without turning the portal on. "
            + "Carry and rotate the bar, or drag a face handle to resize with the opposite face fixed. "
            + "For an end-on face, drag upward, sideways when looking vertically, or push/pull a controller."
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
                { name: "Height", label: "Height (m)", key: "size", axis: 1, low: 0.05, high: 5, decimals: 3 },
                { name: "Depth", label: "Depth (m)", key: "size", axis: 2, low: 0.05, high: 5, decimals: 3 },
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
                    property string current: Number(modelData.axis < 0 ? (page.volume[modelData.key] || 0)
                        : page.volume[modelData.key][modelData.axis]).toFixed(modelData.decimals)
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
        text: "Dimensions: 0.05 to 5 m. Center: -100 to 100 m in OpenVR standing space. "
            + "Angles: -180 to 180 degrees. Edits preserve all other axes, including pitch and roll. "
            + "Place at desk preserves size and tilt. Recheck placement after SteamVR recenters. Placement never turns the portal on."
    }
}
