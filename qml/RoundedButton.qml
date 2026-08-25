import QtQuick
import QtQuick.Controls

Button {
    id: control

    property bool primary: false
    property int radius: 12

    implicitHeight: 40
    implicitWidth: Math.max(88, contentItem.implicitWidth + 32)
    padding: 0

    contentItem: Text {
        text: control.text
        font.pixelSize: 14
        font.weight: control.primary ? Font.DemiBold : Font.Medium
        color: control.primary ? "#FFFFFF" : "#374151"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        radius: control.radius
        color: {
            if (!control.enabled)
                return control.primary ? "#9CA3AF" : "#F3F4F6"
            if (control.pressed)
                return control.primary ? "#1F2937" : "#E5E7EB"
            if (control.hovered)
                return control.primary ? "#374151" : "#F3F4F6"
            return control.primary ? "#111827" : "#FFFFFF"
        }
        border.color: control.primary ? "transparent" : "#D1D5DB"
        border.width: control.primary ? 0 : 1
    }
}
