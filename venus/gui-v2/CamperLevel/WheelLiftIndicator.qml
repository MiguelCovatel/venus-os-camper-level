import QtQuick
import Victron.VenusOS

Item {
	id: root
	property string wheelName
	property real liftMm: NaN
	property color accentColor: Theme.color_font_secondary
	readonly property real safeLift: isNaN(liftMm) ? 0 : Math.max(0, liftMm)
	readonly property real displayLift: Math.round(safeLift / 10) * 10
	readonly property real liftPixels: Math.min(24, displayLift * 24 / 60)

	Rectangle { anchors.fill: parent; radius: 8; color: Theme.color_background_primary }
	Rectangle {
		id: guide; x: 12; y: 9; width: 2; height: 36
		color: Theme.color_card_separator
	}
	Rectangle {
		x: 7; y: guide.y + guide.height; width: 12; height: 2
		color: Theme.color_font_secondary
	}
	Label {
		anchors.horizontalCenter: guide.horizontalCenter; y: 4
		text: "↑"; visible: root.safeLift >= 1
		color: root.accentColor; font.bold: true
	}
	Rectangle {
		id: wheel
		x: 4; y: guide.y + guide.height - height - root.liftPixels
		width: 18; height: 9; radius: 4; color: root.accentColor
		Behavior on y { NumberAnimation { duration: 350; easing.type: Easing.OutCubic } }
	}
	Label {
		anchors { left: wheel.right; leftMargin: 8; right: parent.right; top: parent.top; topMargin: 11 }
		text: root.wheelName; color: Theme.color_font_primary; font.bold: true
	}
	Label {
		anchors { left: wheel.right; leftMargin: 8; right: parent.right; bottom: parent.bottom; bottomMargin: 9 }
		text: isNaN(root.liftMm) ? "--" : "+" + root.displayLift + " mm"
		color: root.accentColor; font.bold: true
	}
}
