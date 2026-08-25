import QtQuick
import QtQuick.Controls

MenuItem {
    id: control

    property bool danger: false

    implicitWidth: Math.max(148, contentItem.implicitWidth + leftPadding + rightPadding)
    implicitHeight: 36
    leftPadding: 14
    rightPadding: 14
    topPadding: 0
    bottomPadding: 0
    hoverEnabled: true

    contentItem: Text {
        text: control.text
        font.pixelSize: 13
        font.weight: Font.Medium
        color: {
            if (!control.enabled)
                return "#9CA3AF"
            if (control.danger)
                return active ? "#DC2626" : "#EF4444"
            return active ? "#111827" : "#4B5563"
        }
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight

        readonly property bool active: control.highlighted || control.hovered
    }

    background: Item {
        implicitWidth: 148
        implicitHeight: 36

        Rectangle {
            anchors.fill: parent
            anchors.leftMargin: 6
            anchors.rightMargin: 6
            anchors.topMargin: 2
            anchors.bottomMargin: 2
            radius: 10
            color: {
                if (!control.enabled)
                    return "transparent"
                if (!(control.highlighted || control.hovered))
                    return "transparent"
                return control.danger ? "#FEF2F2" : "#F3F4F6"
            }
        }
    }
}
