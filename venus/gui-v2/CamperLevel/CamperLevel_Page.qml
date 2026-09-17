import QtQuick
import Victron.VenusOS

Page {
	id: root
	property var device: null
	title: "Camper Level"
	readonly property string serviceUid: BackendConnection.serviceUidFromName(
			"com.victronenergy.switch.camperlevel", 42)
	readonly property bool online: (connected.valid && Number(connected.value) === 1)
			|| (deviceOnline.valid && Number(deviceOnline.value) === 1)
	readonly property int communicationCode: communication.valid ? Number(communication.value) : 2
	readonly property int stabilityCode: levelStability.valid ? Number(levelStability.value) : 3
	readonly property bool imuFault: imuState.valid && Number(imuState.value) === 1

	function numberValue(item) {
		if (!item.valid || item.value === null || item.value === undefined) return NaN
		const value = Number(item.value)
		return isNaN(value) ? NaN : value
	}
	function formatted(item, decimals, suffix) {
		const value = numberValue(item)
		return isNaN(value) ? "--" : value.toFixed(decimals) + suffix
	}
	function qualityText() {
		if (!levelQuality.valid) return "NO DATA"
		const labels = ["LEVEL", "SLIGHTLY UNLEVEL", "UNLEVEL", "NO DATA"]
		const index = Number(levelQuality.value)
		return index >= 0 && index < labels.length ? labels[index] : "NO DATA"
	}
	function zeroStatusText() {
		if (!zeroStatus.valid) return "NOT AVAILABLE"
		switch (Number(zeroStatus.value)) {
		case 1: return "SAVING…"
		case 2: return "SAVED"
		case 3: return "ERROR"
		default: return "READY"
		}
	}
	function headline() {
		if (!online || communicationCode >= 2) return "CAMPER LEVEL SENSOR OFFLINE"
		if (imuFault) return "IMU SENSOR FAULT"
		if (communicationCode === 1) return "COMMUNICATION WARNING"
		if (stabilityCode === 0) return "VEHICLE MOVING"
		if (stabilityCode === 1) return "STABILIZING"
		return qualityText()
	}
	function headlineColor() {
		if (!online || communicationCode >= 2) return Theme.color_critical
		if (imuFault) return Theme.color_critical
		if (communicationCode === 1 || stabilityCode < 2) return Theme.color_warning
		return qualityText() === "LEVEL" ? Theme.color_ok : Theme.color_warning
	}

	GradientListView {
		model: VisibleItemModel {
			PrimaryListLabel { text: root.headline(); color: root.headlineColor(); font.bold: true }
			SettingsListHeader { text: "LEVEL" }
			ListItem {
				contentItem: CamperLevel {
					connected: root.online
					stable: root.stabilityCode === 2
					stateText: root.qualityText()
					pitch: root.numberValue(pitch); roll: root.numberValue(roll)
					frontLeft: root.numberValue(frontLeft); frontRight: root.numberValue(frontRight)
					rearLeft: root.numberValue(rearLeft); rearRight: root.numberValue(rearRight)
				}
			}
			ListText { text: "Pitch"; secondaryText: root.formatted(pitch, 1, "°") }
			ListText { text: "Roll"; secondaryText: root.formatted(roll, 1, "°") }
			ListButton {
				text: "Set current position as LEVEL 0"
				secondaryText: root.zeroStatusText()
				interactive: root.online && pitch.valid && roll.valid && zeroCommand.valid
				onClicked: {
					const current = zeroCommand.valid ? Number(zeroCommand.value) : 0
					const nextRequest = !isFinite(current) || current < 1 || current >= 2147483647
							? 1 : Math.floor(current) + 1
					zeroCommand.setValue(nextRequest)
				}
			}
			PrimaryListLabel {
				text: "Park on a surface you accept as horizontal, then save LEVEL 0. The reference is stored in the ESP32."
				color: Theme.color_font_secondary
			}
			SettingsListHeader { text: "VEHICLE DIMENSIONS" }
			ListSpinBox {
				text: "Wheelbase"; dataItem.uid: root.serviceUid + "/GenericInput/9/Value"
				suffix: " mm"; from: 500; to: 12000; stepSize: 10; decimals: 0
			}
			ListSpinBox {
				text: "Front track (centre to centre)"; dataItem.uid: root.serviceUid + "/GenericInput/10/Value"
				suffix: " mm"; from: 500; to: 4000; stepSize: 10; decimals: 0
			}
			ListSpinBox {
				text: "Rear track (centre to centre)"; dataItem.uid: root.serviceUid + "/GenericInput/11/Value"
				suffix: " mm"; from: 500; to: 4000; stepSize: 10; decimals: 0
			}
			PrimaryListLabel {
				text: root.online
					? "Dimensions are sent to the ESP32 and the four wheel corrections are recalculated."
					: "Dimensions can be saved offline and will be sent when the ESP32 reconnects."
				color: Theme.color_font_secondary
			}
		}
	}

	VeQuickItem { id: connected; uid: root.serviceUid + "/Connected" }
	VeQuickItem { id: pitch; uid: root.serviceUid + "/GenericInput/0/Value" }
	VeQuickItem { id: roll; uid: root.serviceUid + "/GenericInput/1/Value" }
	VeQuickItem { id: frontLeft; uid: root.serviceUid + "/GenericInput/2/Value" }
	VeQuickItem { id: frontRight; uid: root.serviceUid + "/GenericInput/3/Value" }
	VeQuickItem { id: rearLeft; uid: root.serviceUid + "/GenericInput/4/Value" }
	VeQuickItem { id: rearRight; uid: root.serviceUid + "/GenericInput/5/Value" }
	VeQuickItem { id: levelQuality; uid: root.serviceUid + "/GenericInput/6/Value" }
	VeQuickItem { id: levelStability; uid: root.serviceUid + "/GenericInput/7/Value" }
	VeQuickItem { id: deviceOnline; uid: root.serviceUid + "/GenericInput/8/Value" }
	VeQuickItem { id: communication; uid: root.serviceUid + "/GenericInput/12/Value" }
	VeQuickItem { id: imuState; uid: root.serviceUid + "/GenericInput/13/Value" }
	VeQuickItem { id: zeroCommand; uid: root.serviceUid + "/Settings/Level/ZeroCommand" }
	VeQuickItem { id: zeroStatus; uid: root.serviceUid + "/Settings/Level/ZeroStatus" }
}
