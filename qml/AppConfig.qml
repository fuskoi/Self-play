pragma Singleton
import QtQuick
import QtCore

QtObject {
    id: root

    readonly property Settings store: Settings {
        category: "App"

        property string workStart: "09:00"
        property string workEnd: "18:00"
        property real monthlySalary: 15000
        property int payday: 15
        property string themeId: "default"
        property real windowOpacity: 1.0
        property bool alwaysOnTop: true
        property int windowX: -1
        property int windowY: -1
    }

    readonly property var themes: ({
        "default": {
            "name": "清新白",
            "cardBg": "#F3F4F6",
            "windowBg": "#FFFFFF",
            "titleColor": "#9CA3AF",
            "timeColor": "#374151",
            "cardTitle": "#6B7280",
            "cardValue": "#1F2937",
            "cardUnit": "#9CA3AF",
            "accent": "#60A5FA",
            "shadow": "#00000020"
        },
        "warm": {
            "name": "暖橙",
            "cardBg": "#FFF7ED",
            "windowBg": "#FFFBEB",
            "titleColor": "#D97706",
            "timeColor": "#9A3412",
            "cardTitle": "#B45309",
            "cardValue": "#7C2D12",
            "cardUnit": "#D97706",
            "accent": "#F97316",
            "shadow": "#F9731630"
        },
        "mint": {
            "name": "薄荷绿",
            "cardBg": "#ECFDF5",
            "windowBg": "#F0FDFA",
            "titleColor": "#059669",
            "timeColor": "#064E3B",
            "cardTitle": "#047857",
            "cardValue": "#065F46",
            "cardUnit": "#10B981",
            "accent": "#34D399",
            "shadow": "#10B98130"
        },
        "lavender": {
            "name": "薰衣草",
            "cardBg": "#F5F3FF",
            "windowBg": "#FAF5FF",
            "titleColor": "#7C3AED",
            "timeColor": "#4C1D95",
            "cardTitle": "#6D28D9",
            "cardValue": "#5B21B6",
            "cardUnit": "#8B5CF6",
            "accent": "#A78BFA",
            "shadow": "#8B5CF630"
        },
        "night": {
            "name": "暗夜",
            "cardBg": "#1F2937",
            "windowBg": "#111827",
            "titleColor": "#9CA3AF",
            "timeColor": "#F9FAFB",
            "cardTitle": "#9CA3AF",
            "cardValue": "#F3F4F6",
            "cardUnit": "#6B7280",
            "accent": "#38BDF8",
            "shadow": "#00000060"
        }
    })

    readonly property var theme: themes[store.themeId] || themes["default"]

    // 国务院公布的法定放假区间（含调休连休）
    readonly property var legalRestPeriods: [
        { name: "元旦", start: "2025-01-01", end: "2025-01-01" },
        { name: "春节", start: "2025-01-28", end: "2025-02-04" },
        { name: "清明节", start: "2025-04-04", end: "2025-04-06" },
        { name: "劳动节", start: "2025-05-01", end: "2025-05-05" },
        { name: "端午节", start: "2025-05-31", end: "2025-06-02" },
        { name: "国庆中秋", start: "2025-10-01", end: "2025-10-08" },

        { name: "元旦", start: "2026-01-01", end: "2026-01-03" },
        { name: "春节", start: "2026-02-15", end: "2026-02-23" },
        { name: "清明节", start: "2026-04-04", end: "2026-04-06" },
        { name: "劳动节", start: "2026-05-01", end: "2026-05-05" },
        { name: "端午节", start: "2026-06-19", end: "2026-06-21" },
        { name: "中秋节", start: "2026-09-25", end: "2026-09-27" },
        { name: "国庆节", start: "2026-10-01", end: "2026-10-07" }
    ]

    // 调休上班日（周末但需要上班）
    readonly property var legalWorkdays: [
        "2025-01-26",
        "2025-02-08",
        "2025-04-27",
        "2025-09-28",
        "2025-10-11",

        "2026-01-04",
        "2026-02-14",
        "2026-02-28",
        "2026-05-09",
        "2026-09-20",
        "2026-10-10"
    ]

    function pad2(n) {
        return n < 10 ? "0" + n : "" + n
    }

    function dateKey(d) {
        return d.getFullYear() + "-" + pad2(d.getMonth() + 1) + "-" + pad2(d.getDate())
    }

    function parseDate(str) {
        var parts = String(str).split("-")
        return new Date(parseInt(parts[0], 10),
                        parseInt(parts[1], 10) - 1,
                        parseInt(parts[2], 10))
    }

    function dateOnly(d) {
        return new Date(d.getFullYear(), d.getMonth(), d.getDate())
    }

    function parseTime(str) {
        var parts = String(str).split(":")
        var h = parseInt(parts[0], 10)
        var m = parts.length > 1 ? parseInt(parts[1], 10) : 0
        if (isNaN(h)) h = 9
        if (isNaN(m)) m = 0
        return { hour: h, minute: m }
    }

    function isLegalWorkdayOverride(now) {
        var key = dateKey(dateOnly(now))
        for (var i = 0; i < legalWorkdays.length; ++i) {
            if (legalWorkdays[i] === key)
                return true
        }
        return false
    }

    function isInLegalRestPeriod(now) {
        var today = dateOnly(now)
        for (var i = 0; i < legalRestPeriods.length; ++i) {
            var period = legalRestPeriods[i]
            var start = parseDate(period.start)
            var end = parseDate(period.end)
            if (today >= start && today <= end)
                return true
        }
        return false
    }

    function isWeekend(now) {
        var dow = now.getDay()
        return dow === 0 || dow === 6
    }

    function isRestDay(now) {
        if (isLegalWorkdayOverride(now))
            return false
        if (isInLegalRestPeriod(now))
            return true
        return isWeekend(now)
    }

    function daysUntilPayday(now) {
        var day = store.payday
        if (day < 1) day = 1
        if (day > 28) day = 28
        var target = new Date(now.getFullYear(), now.getMonth(), day)
        if (target <= now) {
            target = new Date(now.getFullYear(), now.getMonth() + 1, day)
        }
        return Math.ceil((target - now) / 86400000)
    }

    function daysUntilNextRest(now) {
        if (isRestDay(now))
            return 0
        var today = dateOnly(now)
        for (var i = 1; i <= 366; ++i) {
            var next = new Date(today.getTime() + i * 86400000)
            if (isRestDay(next))
                return i
        }
        return 0
    }

    function nearestHoliday(now) {
        var today = dateOnly(now)
        var bestName = "节日"
        var bestDays = -1

        for (var i = 0; i < legalRestPeriods.length; ++i) {
            var period = legalRestPeriods[i]
            var start = parseDate(period.start)
            if (start < today)
                continue
            var days = Math.round((start - today) / 86400000)
            if (bestDays < 0 || days < bestDays) {
                bestDays = days
                bestName = period.name
            }
        }

        return { name: bestName, days: Math.max(0, bestDays) }
    }

    function mascotSource(now) {
        return isRestDay(now)
                ? "qrc:/qt/qml/countdown/resources/mascot_rest.png"
                : "qrc:/qt/qml/countdown/resources/mascot.png"
    }

    function offWorkRemaining(now) {
        if (isRestDay(now)) {
            return { text: "00:00:00", done: true, workedMs: 0 }
        }

        var end = parseTime(store.workEnd)
        var start = parseTime(store.workStart)
        var endDate = new Date(now.getFullYear(), now.getMonth(), now.getDate(), end.hour, end.minute, 0)
        var startDate = new Date(now.getFullYear(), now.getMonth(), now.getDate(), start.hour, start.minute, 0)

        if (now >= endDate) {
            return { text: "00:00:00", done: true, workedMs: endDate - startDate }
        }
        if (now < startDate) {
            var total = endDate - startDate
            var h = Math.floor(total / 3600000)
            var mi = Math.floor((total % 3600000) / 60000)
            var s = Math.floor((total % 60000) / 1000)
            return {
                text: pad2(h) + ":" + pad2(mi) + ":" + pad2(s),
                done: false,
                workedMs: 0
            }
        }
        var remain = endDate - now
        var hours = Math.floor(remain / 3600000)
        var mins = Math.floor((remain % 3600000) / 60000)
        var secs = Math.floor((remain % 60000) / 1000)
        return {
            text: pad2(hours) + ":" + pad2(mins) + ":" + pad2(secs),
            done: false,
            workedMs: now - startDate
        }
    }

    function workDaysInMonth(now) {
        var year = now.getFullYear()
        var month = now.getMonth()
        var count = 0
        var daysInMonth = new Date(year, month + 1, 0).getDate()
        for (var day = 1; day <= daysInMonth; ++day) {
            if (!isRestDay(new Date(year, month, day)))
                ++count
        }
        return Math.max(1, count)
    }

    function earnedToday(now) {
        if (isRestDay(now))
            return 0

        var salary = Number(store.monthlySalary)
        var days = workDaysInMonth(now)
        var start = parseTime(store.workStart)
        var end = parseTime(store.workEnd)
        var workMs = ((end.hour * 60 + end.minute) - (start.hour * 60 + start.minute)) * 60000
        if (workMs <= 0) return 0

        var info = offWorkRemaining(now)
        var worked = info.workedMs
        if (worked < 0) worked = 0
        if (worked > workMs) worked = workMs

        var daily = salary / days
        return daily * (worked / workMs)
    }

    function formatMoney(v) {
        return Number(v).toFixed(2)
    }
}
