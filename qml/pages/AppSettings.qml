import QtQuick
import Flux

ScrollPage {
    title: "Flux Settings"
    subtitle: "Version " + App.version

    SectionHeader { text: "General" }
    Card {
        SettingRow {
            icon: "power"
            iconTint: Theme.green
            title: "Open at login"
            subtitle: "Starts quietly in the tray so battery alerts keep working."
            Toggle { checked: App.launchAtLogin; onToggled: (v) => App.launchAtLogin = v }
        }
        SettingRow {
            icon: "monitor"
            iconTint: Theme.accent
            title: "Keep running in the tray when closed"
            Toggle { checked: App.closeToTray; onToggled: (v) => App.closeToTray = v }
        }
        SettingRow {
            icon: "wand-sparkles"
            iconTint: Theme.purple
            title: "Reduce motion"
            subtitle: "Calmer, non-bouncy animations."
            Toggle { checked: App.reduceMotion; onToggled: (v) => App.reduceMotion = v }
        }
    }

    SectionHeader { text: "Battery" }
    Card {
        SettingRow {
            icon: "bell"
            iconTint: Theme.red
            title: "Low battery notifications"
            Toggle { checked: App.batteryAlerts; onToggled: (v) => App.batteryAlerts = v }
        }
        SliderRow {
            enabled: App.batteryAlerts
            title: "Warn when the mouse drops below"
            from: 5
            to: 50
            stepSize: 5
            value: App.lowBatteryThreshold
            format: (v) => Math.round(v) + "%"
            onCommitted: (v) => App.lowBatteryThreshold = Math.round(v)
        }
    }

    SectionHeader { text: "About" }
    Card {
        SettingRow {
            title: "Flux " + App.version
            subtitle: "Native Linux settings for LAMZU and MADLIONS devices, built from the vendors' own WebHID protocols. Not affiliated with LAMZU or MADLIONS."
        }
        SettingRow {
            title: "Device access"
            subtitle: "Installed by the package as /usr/lib/udev/rules.d/70-flux.rules"
        }
    }
}
