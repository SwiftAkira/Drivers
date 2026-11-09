#pragma once

#include "LamzuProtocol.h"
#include "core/DeviceBase.h"
#include "core/DeviceManager.h"

#include <QColor>
#include <QTimer>
#include <QVariantList>

#include <map>
#include <memory>

namespace flux {

class LamzuMouse : public DeviceBase {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by DeviceManager")

    // Status / capabilities
    Q_PROPERTY(int batteryLevel READ batteryLevel NOTIFY batteryChanged)
    Q_PROPERTY(bool charging READ charging NOTIFY batteryChanged)
    Q_PROPERTY(bool wireless READ wireless CONSTANT)
    Q_PROPERTY(QString mouseFirmware READ mouseFirmware NOTIFY infoChanged)
    Q_PROPERTY(QString receiverFirmware READ receiverFirmware NOTIFY infoChanged)
    Q_PROPERTY(QString sensorName READ sensorName NOTIFY infoChanged)
    Q_PROPERTY(int dpiMin READ dpiMin CONSTANT)
    Q_PROPERTY(int dpiMax READ dpiMax NOTIFY infoChanged)
    Q_PROPERTY(int dpiStep READ dpiStep NOTIFY infoChanged)
    Q_PROPERTY(int maxStages READ maxStages CONSTANT)
    Q_PROPERTY(QVariantList lodOptions READ lodOptions NOTIFY infoChanged)
    Q_PROPERTY(QVariantList pollingRates READ pollingRates CONSTANT)
    Q_PROPERTY(QVariantList sleepOptions READ sleepOptions CONSTANT)
    Q_PROPERTY(bool trackingSupported READ trackingSupported NOTIFY infoChanged)
    Q_PROPERTY(int profileCount READ profileCount CONSTANT)

    // Settings of the active on-board profile
    Q_PROPERTY(int profile READ profile NOTIFY settingsChanged)
    Q_PROPERTY(int pollingRate READ pollingRate NOTIFY settingsChanged)
    Q_PROPERTY(QVariantList stages READ stages NOTIFY settingsChanged)
    Q_PROPERTY(int activeStage READ activeStage NOTIFY settingsChanged)
    Q_PROPERTY(bool dpiXY READ dpiXY NOTIFY settingsChanged)
    Q_PROPERTY(double lod READ lod NOTIFY settingsChanged)
    Q_PROPERTY(bool motionSync READ motionSync NOTIFY settingsChanged)
    Q_PROPERTY(bool angleSnap READ angleSnap NOTIFY settingsChanged)
    Q_PROPERTY(bool rippleControl READ rippleControl NOTIFY settingsChanged)
    Q_PROPERTY(bool hyperMode READ hyperMode NOTIFY settingsChanged)
    Q_PROPERTY(bool trackingMode READ trackingMode NOTIFY settingsChanged)
    Q_PROPERTY(int angleTune READ angleTune NOTIFY settingsChanged)
    Q_PROPERTY(int debounce READ debounce NOTIFY settingsChanged)
    Q_PROPERTY(int sleepSeconds READ sleepSeconds NOTIFY settingsChanged)
    Q_PROPERTY(QVariantList buttons READ buttons NOTIFY buttonsChanged)
    Q_PROPERTY(QVariantList macros READ macros NOTIFY macrosChanged)
    Q_PROPERTY(bool macrosLoaded READ macrosLoaded NOTIFY macrosChanged)

public:
    LamzuMouse(const HidInfo &info, QObject *parent);
    ~LamzuMouse() override;

    static DeviceMatch match();

    void start() override;
    void refresh() override;
    QVariantMap exportSettings() const override;
    void importSettings(const QVariantMap &s) override;

    int batteryLevel() const { return m_battery; }
    bool charging() const { return m_charging; }
    bool wireless() const { return m_wireless; }
    QString mouseFirmware() const { return m_mouseFw; }
    QString receiverFirmware() const { return m_receiverFw; }
    QString sensorName() const;
    int dpiMin() const { return 50; }
    int dpiMax() const { return m_dpiMax; }
    int dpiStep() const { return m_dpiStep; }
    int maxStages() const { return 5; }
    QVariantList lodOptions() const;
    QVariantList pollingRates() const;
    QVariantList sleepOptions() const;
    bool trackingSupported() const { return m_fwNum >= 16; }
    int profileCount() const { return 3; }

    int profile() const { return m_s.profile; }
    int pollingRate() const { return m_s.polling; }
    QVariantList stages() const;
    int activeStage() const { return m_s.active; }
    bool dpiXY() const { return m_s.xy; }
    double lod() const { return m_s.lod; }
    bool motionSync() const { return m_s.motion; }
    bool angleSnap() const { return m_s.snap; }
    bool rippleControl() const { return m_s.ripple; }
    bool hyperMode() const { return m_s.hyper; }
    bool trackingMode() const { return m_s.tracking; }
    int angleTune() const { return m_s.tune; }
    int debounce() const { return m_s.debounce; }
    int sleepSeconds() const { return m_s.sleep; }
    QVariantList buttons() const;
    QVariantList macros() const;
    bool macrosLoaded() const { return m_macrosLoaded; }

    Q_INVOKABLE void setProfile(int p);
    Q_INVOKABLE void setPollingRate(int hz);
    Q_INVOKABLE void setStageCount(int n);
    Q_INVOKABLE void setStage(int index, int x, int y);
    Q_INVOKABLE void setActiveStage(int stage1);
    Q_INVOKABLE void setDpiXY(bool on);
    Q_INVOKABLE void setStageColor(int index, const QColor &c);
    Q_INVOKABLE void setLod(double mm);
    Q_INVOKABLE void setMotionSync(bool on);
    Q_INVOKABLE void setAngleSnap(bool on);
    Q_INVOKABLE void setRippleControl(bool on);
    Q_INVOKABLE void setHyperMode(bool on);
    Q_INVOKABLE void setTrackingMode(bool on);
    Q_INVOKABLE void setAngleTune(int deg);
    Q_INVOKABLE void setDebounce(int ms);
    Q_INVOKABLE void setSleepSeconds(int s);
    Q_INVOKABLE bool setButtonAction(int buttonId, const QVariantMap &action);
    Q_INVOKABLE void resetButton(int buttonId);
    Q_INVOKABLE QString describeAction(const QVariantMap &action) const;
    Q_INVOKABLE void saveMacro(int id, const QString &name, const QVariantList &events);
    Q_INVOKABLE void deleteMacro(int id);
    Q_INVOKABLE int freeMacroId() const;
    Q_INVOKABLE QString macroName(int id) const;
    Q_INVOKABLE void factoryReset();
    Q_INVOKABLE void applyOsuPreset();

signals:
    void batteryChanged();
    void settingsChanged();
    void buttonsChanged();
    void macrosChanged();

private:
    struct State {
        int profile = 1;
        int polling = 1000;
        std::vector<lamzu::Stage> stages{{400, 400}, {800, 800}, {1600, 1600}, {3200, 3200}};
        std::vector<lamzu::Rgb> colors;
        int active = 1;
        bool xy = false;
        double lod = 1.0;
        bool motion = false, snap = false, ripple = false, hyper = false, tracking = false;
        int tune = 0;
        int debounce = 0;
        int sleep = 60;
        std::map<int, lamzu::ButtonAction> buttons;
    };
    struct Ident {
        bool ok = false;
        QString mouseFw, receiverFw;
        int fwNum = 0;
        int sensor = 0;
    };

    // I/O-thread helpers
    bool openIo();
    bool probeOnline();
    Ident readIdent();
    State readState(int sensor, bool tracking);
    std::map<int, std::vector<uint8_t>> readMacros();
    void ioIdle();

    // GUI-thread helpers
    void fullRead();
    void applyIdent(const Ident &id);
    void applyState(const State &s);
    void setBattery(int level, bool charging);
    void write(std::function<bool(lamzu::Protocol &)> fn, const QString &what);
    void writeStages();
    void handleEvent(const lamzu::Event &e);
    void schedulePolls();

    static QVariantMap actionToMap(const lamzu::ButtonAction &a);
    static lamzu::ButtonAction mapToAction(const QVariantMap &m);
    static lamzu::ButtonAction defaultAction(int buttonId);
    static QVariantList decodeMacro(const std::vector<uint8_t> &raw);
    static std::vector<uint8_t> encodeMacro(const QVariantList &events);
    static lamzu::Rgb defaultColor(int index);

    HidInfo m_info;
    std::string m_notifyPath;
    HidDevice m_cfg;   // I/O thread only
    HidDevice m_ntf;   // I/O thread only
    std::unique_ptr<lamzu::Protocol> m_proto;

    State m_s;
    bool m_wireless = false;
    int m_battery = -1;
    bool m_charging = false;
    QString m_mouseFw, m_receiverFw;
    int m_fwNum = 0;
    int m_sensor = 0;
    int m_dpiMax = 30000;
    int m_dpiStep = 50;
    std::map<int, std::vector<uint8_t>> m_macros;
    bool m_macrosLoaded = false;
    bool m_reading = false;
    int m_batteryFailures = 0;

    QTimer m_probeTimer;
    QTimer m_batteryTimer;
};

} // namespace flux
