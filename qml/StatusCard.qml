import QtQuick

Rectangle {
    id: root

    property string title: ""
    property string value: ""
    property string unit: ""
    property color titleColor: AppConfig.theme.cardTitle
    property color valueColor: AppConfig.theme.cardValue
    property color unitColor: AppConfig.theme.cardUnit

    width: 46
    height: 56
    radius: 8
    color: AppConfig.theme.cardBg

    Column {
        anchors.centerIn: parent
        spacing: 1

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.title
            color: root.titleColor
            font.pixelSize: 9
            font.weight: Font.Medium
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.value
            color: root.valueColor
            font.pixelSize: 14
            font.bold: true
            font.family: "Segoe UI"
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.unit
            color: root.unitColor
            font.pixelSize: 8
        }
    }
}
