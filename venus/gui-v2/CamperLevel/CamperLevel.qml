import QtQuick
import Victron.VenusOS

Item {
	id: root
	property bool connected
	property bool stable
	property string stateText: "NO DATA"
	property real pitch: NaN
	property real roll: NaN
	property real frontLeft: NaN
	property real frontRight: NaN
	property real rearLeft: NaN
	property real rearRight: NaN
	implicitWidth: Theme.geometry_listItem_width
	implicitHeight: 238

	function clamp(value, minimum, maximum) {
		return Math.max(minimum, Math.min(maximum, value))
	}
	function wheelColor(value) {
		if (isNaN(value)) return Theme.color_font_secondary
		if (value <= 2) return Theme.color_ok
		if (value >= 50) return Theme.color_critical
		return Theme.color_warning
	}

	Label {
		id: frontLabel
		anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter
		text: "FRONT"; font.bold: true; color: Theme.color_font_secondary
	}
	Item {
		id: diagram
		anchors { top: frontLabel.bottom; topMargin: 6; left: parent.left; right: parent.right; bottom: stateLabel.top; bottomMargin: 8 }
		readonly property real wheelWidth: Math.max(90, Math.min(145, width * 0.29))
		Rectangle {
			id: chassis
			anchors.centerIn: parent
			width: Math.max(105, Math.min(210, parent.width * 0.34)); height: 132; radius: 20
			color: Theme.color_background_secondary; border.width: 2
			border.color: !root.connected ? Theme.color_critical : root.stable ? Theme.color_ok : Theme.color_warning
			Rectangle {
				anchors.horizontalCenter: parent.horizontalCenter
				width: 1; height: parent.height - 28; color: Theme.color_card_separator
			}
			Rectangle {
				anchors.verticalCenter: parent.verticalCenter
				width: parent.width - 28; height: 1; color: Theme.color_card_separator
			}
			Rectangle {
				width: 18; height: 18; radius: 9
				color: root.stable ? Theme.color_ok : Theme.color_primary
				x: parent.width / 2 - width / 2 + (isNaN(root.roll) ? 0 : root.clamp(-root.roll * 8, -parent.width / 2 + 18, parent.width / 2 - 18))
				y: parent.height / 2 - height / 2 + (isNaN(root.pitch) ? 0 : root.clamp(root.pitch * 8, -parent.height / 2 + 18, parent.height / 2 - 18))
				Behavior on x { NumberAnimation { duration: 250 } }
				Behavior on y { NumberAnimation { duration: 250 } }
			}
		}
		WheelLiftIndicator {
			anchors.left: parent.left; anchors.top: parent.top
			width: parent.wheelWidth; height: 64; wheelName: "FL"
			liftMm: root.frontLeft; accentColor: root.wheelColor(root.frontLeft)
		}
		WheelLiftIndicator {
			anchors.right: parent.right; anchors.top: parent.top
			width: parent.wheelWidth; height: 64; wheelName: "FR"
			liftMm: root.frontRight; accentColor: root.wheelColor(root.frontRight)
		}
		WheelLiftIndicator {
			anchors.left: parent.left; anchors.bottom: parent.bottom
			width: parent.wheelWidth; height: 64; wheelName: "RL"
			liftMm: root.rearLeft; accentColor: root.wheelColor(root.rearLeft)
		}
		WheelLiftIndicator {
			anchors.right: parent.right; anchors.bottom: parent.bottom
			width: parent.wheelWidth; height: 64; wheelName: "RR"
			liftMm: root.rearRight; accentColor: root.wheelColor(root.rearRight)
		}
	}
	Label {
		id: stateLabel
		anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter
		text: root.connected ? root.stateText : "OFFLINE"; font.bold: true
		color: !root.connected ? Theme.color_critical : root.stable ? Theme.color_ok : Theme.color_warning
	}
}
