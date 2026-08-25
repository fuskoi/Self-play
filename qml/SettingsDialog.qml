import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Window {
    id: root

    width: 480
    height: 660
    minimumWidth: 460
    minimumHeight: 560
    title: "设置"
    color: "#FFFFFF"
    modality: Qt.ApplicationModal
    flags: Qt.Dialog | Qt.WindowTitleHint | Qt.WindowCloseButtonHint | Qt.WindowStaysOnTopHint

    signal closed()

    function open() {
        x = Math.round((Screen.width - width) / 2)
        y = Math.round((Screen.height - height) / 2)
        reloadFields()
        show()
        raise()
        requestActivate()
    }

    function closeDialog() {
        hide()
        closed()
    }

    component SectionCard: ColumnLayout {
        property string title: ""
        default property alias content: contentLayout.data

        spacing: 10
        width: parent ? parent.width : implicitWidth

        Text {
            text: title
            font.pixelSize: 14
            font.weight: Font.DemiBold
            color: "#374151"
        }

        Rectangle {
            Layout.fillWidth: true
            radius: 14
            color: "#F9FAFB"
            border.color: "#EEF2F7"
            border.width: 1
            implicitHeight: contentLayout.implicitHeight + 24

            ColumnLayout {
                id: contentLayout
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10
            }
        }
    }

    component FieldRow: RowLayout {
        property string label: ""
        default property alias field: fieldSlot.data

        spacing: 10
        Layout.fillWidth: true

        Text {
            text: label
            color: "#6B7280"
            font.pixelSize: 13
            Layout.preferredWidth: 72
            Layout.alignment: Qt.AlignVCenter
        }

        Item {
            id: fieldSlot
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            Layout.minimumHeight: 40
        }
    }

    component StyledTextField: TextField {
        implicitHeight: 40
        padding: 10
        font.pixelSize: 14
        color: "#111827"
        placeholderTextColor: "#9CA3AF"
        selectionColor: "#BFDBFE"
        selectedTextColor: "#111827"

        background: Rectangle {
            radius: 10
            color: "#FFFFFF"
            border.color: parent.activeFocus ? "#60A5FA" : "#D1D5DB"
            border.width: 1
        }
    }

    component StyledComboBox: ComboBox {
        id: control
        implicitHeight: 40
        Layout.fillWidth: true

        background: Rectangle {
            radius: 10
            color: "#FFFFFF"
            border.color: control.pressed || control.activeFocus ? "#60A5FA" : "#D1D5DB"
            border.width: 1
        }

        contentItem: Text {
            leftPadding: 12
            rightPadding: 28
            text: control.displayText
            font.pixelSize: 14
            color: "#111827"
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        indicator: Text {
            x: control.width - width - 12
            y: (control.height - height) / 2
            text: "▾"
            font.pixelSize: 12
            color: "#6B7280"
        }

        delegate: ItemDelegate {
            id: itemDelegate
            required property string label
            required property string value
            width: control.width
            height: 40
            padding: 8

            contentItem: Text {
                text: itemDelegate.label
                color: itemDelegate.highlighted ? "#1D4ED8" : "#111827"
                font.pixelSize: 14
                verticalAlignment: Text.AlignVCenter
                leftPadding: 4
            }

            background: Rectangle {
                radius: 8
                color: itemDelegate.highlighted ? "#EFF6FF" : "transparent"
            }
        }

        popup: Popup {
            y: control.height + 4
            width: control.width
            padding: 6
            implicitHeight: Math.min(listView.contentHeight + 12, 240)

            background: Rectangle {
                radius: 12
                color: "#FFFFFF"
                border.color: "#E5E7EB"
                border.width: 1
            }

            contentItem: ListView {
                id: listView
                clip: true
                implicitHeight: contentHeight
                model: control.delegateModel
                currentIndex: control.highlightedIndex
                boundsBehavior: Flickable.StopAtBounds
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14

        Text {
            text: "下班倒计时设置"
            font.pixelSize: 22
            font.bold: true
            color: "#111827"
            Layout.fillWidth: true
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            ScrollBar.vertical.policy: ScrollBar.AsNeeded

            ColumnLayout {
                width: root.width - 40
                spacing: 16

                SectionCard {
                    title: "工作时间"
                    FieldRow {
                        label: "上班"
                        StyledTextField {
                            id: startField
                            width: parent.width
                            placeholderText: "09:00"
                        }
                    }
                    FieldRow {
                        label: "下班"
                        StyledTextField {
                            id: endField
                            width: parent.width
                            placeholderText: "18:00"
                        }
                    }
                }

                SectionCard {
                    title: "薪资与发薪"
                    FieldRow {
                        label: "月薪"
                        StyledTextField {
                            id: salaryField
                            width: parent.width
                            validator: DoubleValidator { bottom: 0; decimals: 2 }
                        }
                    }
                    FieldRow {
                        label: "发薪日"
                        StyledTextField {
                            id: paydayField
                            width: parent.width
                            inputMethodHints: Qt.ImhDigitsOnly
                            validator: IntValidator { bottom: 1; top: 28 }
                        }
                    }
                }

                SectionCard {
                    title: "主题与窗口"
                    FieldRow {
                        label: "主题"
                        StyledComboBox {
                            id: themeBox
                            model: [
                                { label: "清新白", value: "default" },
                                { label: "暖橙", value: "warm" },
                                { label: "薄荷绿", value: "mint" },
                                { label: "薰衣草", value: "lavender" },
                                { label: "暗夜", value: "night" }
                            ]
                            textRole: "label"
                            valueRole: "value"
                        }
                    }
                    FieldRow {
                        label: "透明度"
                        RowLayout {
                            width: parent.width
                            spacing: 10
                            Slider {
                                id: opacitySlider
                                from: 0.5
                                to: 1.0
                                stepSize: 0.05
                                Layout.fillWidth: true
                            }
                            Text {
                                text: Math.round(opacitySlider.value * 100) + "%"
                                color: "#6B7280"
                                font.pixelSize: 13
                                Layout.preferredWidth: 42
                            }
                        }
                    }
                    CheckBox {
                        id: topCheck
                        text: "始终置顶"
                        Layout.leftMargin: 4
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Item { Layout.fillWidth: true }

            RoundedButton {
                text: "取消"
                onClicked: root.closeDialog()
            }

            RoundedButton {
                text: "保存"
                primary: true
                onClicked: {
                    AppConfig.store.workStart = startField.text.trim() || "09:00"
                    AppConfig.store.workEnd = endField.text.trim() || "18:00"
                    AppConfig.store.monthlySalary = parseFloat(salaryField.text) || 0
                    AppConfig.store.payday = clampInt(paydayField.text, 1, 28, 15)
                    AppConfig.store.themeId = themeBox.currentValue
                    AppConfig.store.windowOpacity = opacitySlider.value
                    AppConfig.store.alwaysOnTop = topCheck.checked
                    root.closeDialog()
                }
            }
        }
    }

    function clampInt(text, min, max, fallback) {
        var value = parseInt(String(text).trim(), 10)
        if (isNaN(value))
            return fallback
        return Math.max(min, Math.min(max, value))
    }

    function reloadFields() {
        startField.text = AppConfig.store.workStart
        endField.text = AppConfig.store.workEnd
        salaryField.text = String(AppConfig.store.monthlySalary)
        paydayField.text = String(AppConfig.store.payday)
        var ids = ["default", "warm", "mint", "lavender", "night"]
        themeBox.currentIndex = Math.max(0, ids.indexOf(AppConfig.store.themeId))
        opacitySlider.value = AppConfig.store.windowOpacity
        topCheck.checked = AppConfig.store.alwaysOnTop
    }

    onClosing: function(close) {
        close.accepted = false
        closeDialog()
    }
}
