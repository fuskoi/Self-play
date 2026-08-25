import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window

    property bool dragging: false
    property real dragOffsetX: 0
    property real dragOffsetY: 0

    width: 348
    height: 164
    minimumWidth: 330
    minimumHeight: 150
    visible: true
    title: "下班倒计时"
    color: "transparent"
    flags: Qt.FramelessWindowHint
           | Qt.Window
           | (AppConfig.store.alwaysOnTop ? Qt.WindowStaysOnTopHint : Qt.Window)

    Component.onCompleted: {
        if (AppConfig.store.windowX >= 0 && AppConfig.store.windowY >= 0) {
            x = AppConfig.store.windowX
            y = AppConfig.store.windowY
        } else {
            x = Screen.width - width - 40
            y = 80
        }
        tick()
    }

    function tick() {
        var now = new Date()
        var resting = AppConfig.isRestDay(now)

        paydayCard.value = String(AppConfig.daysUntilPayday(now))
        weekendCard.title = "休息"
        weekendCard.value = String(AppConfig.daysUntilNextRest(now))
        var holiday = AppConfig.nearestHoliday(now)
        holidayCard.title = holiday.name
        holidayCard.value = String(holiday.days)
        mascotImage.source = AppConfig.mascotSource(now)

        if (resting) {
            statusLabel.text = ""
            countdownText.text = "休息时间"
            countdownText.font.pixelSize = 24
            earnedCard.value = "0.00"
            return
        }

        var remain = AppConfig.offWorkRemaining(now)
        countdownText.font.pixelSize = 28
        countdownText.text = remain.text
        earnedCard.value = AppConfig.formatMoney(AppConfig.earnedToday(now))
        statusLabel.text = remain.done ? "已下班，好好休息" : "下班还有"
    }

    function savePos() {
        AppConfig.store.windowX = Math.round(window.x)
        AppConfig.store.windowY = Math.round(window.y)
    }

    Timer {
        interval: 1000
        running: true
        repeat: true
        onTriggered: window.tick()
    }

    Rectangle {
        id: card
        anchors.fill: parent
        anchors.margins: 6
        radius: 18
        color: AppConfig.theme.windowBg
        opacity: AppConfig.store.windowOpacity
        border.color: "#00000018"
        border.width: 1

        Rectangle {
            anchors.fill: parent
            anchors.margins: -1
            z: -1
            radius: parent.radius + 1
            color: "#00000010"
        }

        MouseArea {
            id: dragArea
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            hoverEnabled: true

            onPressed: function(mouse) {
                if (mouse.button === Qt.LeftButton) {
                    window.dragging = true
                    window.dragOffsetX = mouse.x
                    window.dragOffsetY = mouse.y
                }
            }
            onPositionChanged: function(mouse) {
                if (window.dragging && (mouse.buttons & Qt.LeftButton)) {
                    window.x += mouse.x - window.dragOffsetX
                    window.y += mouse.y - window.dragOffsetY
                }
            }
            onReleased: function(mouse) {
                if (mouse.button === Qt.LeftButton) {
                    window.dragging = false
                    window.savePos()
                }
            }
            onClicked: function(mouse) {
                if (mouse.button === Qt.RightButton)
                    contextMenu.popup()
            }
            onDoubleClicked: settingsDialog.open()
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 4
            anchors.topMargin: 14
            anchors.bottomMargin: 12
            spacing: 0
            z: 1

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 10

                ColumnLayout {
                    spacing: 1
                    Text {
                        id: statusLabel
                        text: "下班还有"
                        color: AppConfig.theme.titleColor
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                    }
                    Text {
                        id: countdownText
                        text: "00:00:00"
                        color: AppConfig.theme.timeColor
                        font.pixelSize: 28
                        font.bold: true
                        font.family: "DIN Alternate, Segoe UI, Microsoft YaHei UI"
                        font.letterSpacing: 0.6
                    }
                }

                Row {
                    spacing: 5
                    Layout.fillWidth: true

                    StatusCard {
                        id: paydayCard
                        title: "发薪"
                        value: "0"
                        unit: "天"
                    }
                    StatusCard {
                        id: weekendCard
                        title: "休息"
                        value: "0"
                        unit: "天"
                    }
                    StatusCard {
                        id: holidayCard
                        title: "节日"
                        value: "0"
                        unit: "天"
                    }
                    StatusCard {
                        id: earnedCard
                        title: "今天赚了"
                        value: "0.00"
                        unit: "¥"
                        width: 62
                    }
                }
            }

            Item {
                Layout.preferredWidth: 124
                Layout.fillHeight: true
                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight

                Image {
                    id: mascotImage
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.rightMargin: -4
                    width: 122
                    height: 122
                    fillMode: Image.PreserveAspectFit
                    source: "qrc:/qt/qml/countdown/resources/mascot.png"
                    smooth: true
                    mipmap: true
                    asynchronous: true
                }
            }
        }
    }

    Menu {
        id: contextMenu

        width: 156
        padding: 8
        topPadding: 8
        bottomPadding: 8

        background: Rectangle {
            implicitWidth: 156
            implicitHeight: 40
            radius: 14
            color: "#FFFFFF"
            border.color: "#E5E7EB"
            border.width: 1

            Rectangle {
                anchors.fill: parent
                anchors.margins: -2
                z: -1
                radius: parent.radius + 2
                color: "#00000014"
            }
        }

        enter: Transition {
            NumberAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: 120
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                property: "scale"
                from: 0.96
                to: 1
                duration: 140
                easing.type: Easing.OutCubic
            }
        }

        exit: Transition {
            NumberAnimation {
                property: "opacity"
                from: 1
                to: 0
                duration: 90
                easing.type: Easing.InCubic
            }
        }

        SoftMenuItem {
            text: "设置"
            onTriggered: settingsDialog.open()
        }
        SoftMenuItem {
            text: "最小化"
            onTriggered: window.showMinimized()
        }
        SoftMenuItem {
            text: AppConfig.store.alwaysOnTop ? "取消置顶" : "始终置顶"
            onTriggered: AppConfig.store.alwaysOnTop = !AppConfig.store.alwaysOnTop
        }

        MenuSeparator {
            contentItem: Rectangle {
                implicitWidth: 120
                implicitHeight: 1
                color: "#EEF2F7"
            }
            leftPadding: 12
            rightPadding: 12
            topPadding: 6
            bottomPadding: 6
        }

        SoftMenuItem {
            text: "关闭"
            danger: true
            onTriggered: {
                window.savePos()
                Qt.quit()
            }
        }
    }

    SettingsDialog {
        id: settingsDialog
        onClosed: window.tick()
    }
}
