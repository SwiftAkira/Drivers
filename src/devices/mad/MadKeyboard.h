#pragma once

#include "LightEngine.h"
#include "app/AudioTap.h"
#include "MadProtocol.h"
#include "core/DeviceBase.h"
#include "core/DeviceManager.h"

#include <QColor>
#include <QElapsedTimer>
#include <QHash>
#include <QVariantList>

#include <atomic>
#include <memory>

namespace flux {

class MadKeyboard : public DeviceBase {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by DeviceManager")

    // Geometry / capabilities
    Q_PROPERTY(QVariantList layout READ layout CONSTANT)          // [{index,row,col,x,y,w,h}]
    Q_PROPERTY(int rows READ rows CONSTANT)
    Q_PROPERTY(int cols READ cols CONSTANT)
    Q_PROPERTY(int layerCount READ layerCount CONSTANT)
    Q_PROPERTY(QStringList layerNames READ layerNames CONSTANT)
    Q_PROPERTY(double travelMax READ travelMax CONSTANT)
    Q_PROPERTY(double actuationMin READ actuationMin CONSTANT)
    Q_PROPERTY(double actuationMax READ actuationMax CONSTANT)
    Q_PROPERTY(double rtMin READ rtMin CONSTANT)
    Q_PROPERTY(double rtMax READ rtMax CONSTANT)
    Q_PROPERTY(double deadZoneMax READ deadZoneMax CONSTANT)
    Q_PROPERTY(QVariantList effects READ effects CONSTANT)        // [{id, name, usesColor, usesSpeed}]
    Q_PROPERTY(int speedMax READ speedMax CONSTANT)
    Q_PROPERTY(int brightnessMax READ brightnessMax CONSTANT)
    Q_PROPERTY(int socdSlotCount READ socdSlotCount CONSTANT)
    Q_PROPERTY(int dksSlotCount READ dksSlotCount CONSTANT)
    Q_PROPERTY(int tapDanceSlotCount READ tapDanceSlotCount CONSTANT)

    // Settings
    Q_PROPERTY(QVariantList actuation READ actuation NOTIFY travelSettingsChanged)   // mm per key
    Q_PROPERTY(QVariantList rapidTrigger READ rapidTrigger NOTIFY travelSettingsChanged) // [{on, press, release}]
    Q_PROPERTY(double deadZoneTop READ deadZoneTop NOTIFY travelSettingsChanged)
    Q_PROPERTY(double deadZoneBottom READ deadZoneBottom NOTIFY travelSettingsChanged)
    Q_PROPERTY(QVariantList keymap READ keymap NOTIFY keymapChanged)                // [layer][index] keycodes
    Q_PROPERTY(bool wasdSwap READ wasdSwap NOTIFY featuresChanged)
    Q_PROPERTY(bool macMode READ macMode NOTIFY featuresChanged)
    Q_PROPERTY(bool winLock READ winLock NOTIFY featuresChanged)
    Q_PROPERTY(bool nkro READ nkro NOTIFY featuresChanged)
    Q_PROPERTY(bool bottomOptimize READ bottomOptimize NOTIFY featuresChanged)
    Q_PROPERTY(bool gameMode READ gameMode NOTIFY featuresChanged)
    Q_PROPERTY(int defaultLayer READ defaultLayer NOTIFY featuresChanged)
    Q_PROPERTY(int effect READ effect NOTIFY lightingChanged)
    Q_PROPERTY(int speed READ speed NOTIFY lightingChanged)
    Q_PROPERTY(int brightness READ brightness NOTIFY lightingChanged)
    Q_PROPERTY(QColor lightColor READ lightColor NOTIFY lightingChanged)
    Q_PROPERTY(QVariantList keyColors READ keyColors NOTIFY lightingChanged)
    Q_PROPERTY(QVariantList socdSlots READ socdSlots NOTIFY advancedChanged)
    Q_PROPERTY(QVariantList dksSlots READ dksSlots NOTIFY advancedChanged)
    Q_PROPERTY(QVariantList tapDances READ tapDances NOTIFY advancedChanged)
    Q_PROPERTY(bool advancedLoaded READ advancedLoaded NOTIFY advancedChanged)
    Q_PROPERTY(QVariantList macros READ macros NOTIFY macrosChanged)
    Q_PROPERTY(int macroCapacity READ macroCapacity NOTIFY macrosChanged)
    Q_PROPERTY(int macroSlots READ macroSlots NOTIFY macrosChanged)

    // Live travel
    Q_PROPERTY(bool liveTravel READ liveTravel WRITE setLiveTravel NOTIFY liveTravelChanged)
    Q_PROPERTY(QVariantList travel READ travel NOTIFY travelChanged)                // mm per key
    Q_PROPERTY(double travelFps READ travelFps NOTIFY travelChanged)
    Q_PROPERTY(bool calibrating READ calibrating NOTIFY calibratingChanged)

    // Flux live lighting (host-rendered, streamed per key)
    Q_PROPERTY(QVariantList fluxEffects READ fluxEffects CONSTANT)
    Q_PROPERTY(QString fluxEffect READ fluxEffect NOTIFY fluxChanged)
    Q_PROPERTY(double fluxSpeed READ fluxSpeed NOTIFY fluxChanged)
    Q_PROPERTY(double fluxIntensity READ fluxIntensity NOTIFY fluxChanged)
    Q_PROPERTY(QVariantList fluxPalette READ fluxPalette NOTIFY fluxChanged)
    Q_PROPERTY(QVariantList liveColors READ liveColors NOTIFY liveColorsChanged)
    Q_PROPERTY(double fluxFps READ fluxFps NOTIFY liveColorsChanged)
    Q_PROPERTY(bool boxLightSupported READ boxLightSupported CONSTANT)
    Q_PROPERTY(bool osuMode READ osuMode WRITE setOsuMode NOTIFY osuChanged)
    Q_PROPERTY(QVariantList osuKeys READ osuKeys NOTIFY osuChanged)
    Q_PROPERTY(QString osuKeysText READ osuKeysText NOTIFY osuChanged)
    Q_PROPERTY(int boxMode READ boxMode NOTIFY lightingChanged)
    Q_PROPERTY(bool boxRainbow READ boxRainbow NOTIFY lightingChanged)
    Q_PROPERTY(int boxBrightness READ boxBrightness NOTIFY lightingChanged)
    Q_PROPERTY(int boxSpeed READ boxSpeed NOTIFY lightingChanged)
    Q_PROPERTY(QColor boxColor READ boxColor NOTIFY lightingChanged)

public:
    MadKeyboard(const HidInfo &info, QObject *parent);
    ~MadKeyboard() override;

    static DeviceMatch match();

    void start() override;
    void refresh() override;
    QVariantMap exportSettings() const override;
    void importSettings(const QVariantMap &s) override;

    QVariantList layout() const { return m_layout; }
    int rows() const { return mad::kRows; }
    int cols() const { return mad::kCols; }
    int layerCount() const { return mad::kLayers; }
    QStringList layerNames() const;
    double travelMax() const { return m_travelMax; }
    double actuationMin() const { return m_actMin; }
    double actuationMax() const { return m_actMax; }
    double rtMin() const { return m_rtMin; }
    double rtMax() const { return m_rtMax; }
    double deadZoneMax() const { return m_dzMax; }
    QVariantList effects() const { return m_effects; }
    int speedMax() const { return m_speedMax; }
    int brightnessMax() const { return m_brightnessMax; }
    int socdSlotCount() const { return 20; }
    int dksSlotCount() const { return 20; }
    int tapDanceSlotCount() const { return 40; }

    QVariantList actuation() const;
    QVariantList rapidTrigger() const;
    double deadZoneTop() const { return m_dead.top / 100.0; }
    double deadZoneBottom() const { return m_dead.bottom / 100.0; }
    QVariantList keymap() const;
    bool wasdSwap() const { return m_features.wasdSwap; }
    bool macMode() const { return m_features.macMode; }
    bool winLock() const { return m_features.winLock; }
    bool nkro() const { return m_features.nkro; }
    bool bottomOptimize() const { return m_bottomOptimize; }
    bool gameMode() const { return m_gameMode; }
    int defaultLayer() const { return m_defaultLayer; }
    int effect() const { return m_light.effect; }
    int speed() const { return m_light.speed; }
    int brightness() const { return m_light.brightness; }
    QColor lightColor() const { return QColor(m_light.r, m_light.g, m_light.b); }
    QVariantList keyColors() const;
    QVariantList socdSlots() const;
    QVariantList dksSlots() const;
    QVariantList tapDances() const;
    bool advancedLoaded() const { return m_advancedLoaded; }
    QVariantList macros() const;
    int macroCapacity() const { return m_macroBufSize; }
    int macroSlots() const { return m_macroCount; }
    bool liveTravel() const { return m_live; }
    void setLiveTravel(bool on);
    QVariantList travel() const { return m_travel; }
    double travelFps() const { return m_travelFps; }
    bool calibrating() const { return m_calibrating; }

    QVariantList fluxEffects() const;
    QString fluxEffect() const { return m_fxId; }
    double fluxSpeed() const { return m_fxSpeed; }
    double fluxIntensity() const { return m_fxIntensity; }
    QVariantList fluxPalette() const;
    QVariantList liveColors() const { return m_liveColors; }
    double fluxFps() const { return m_fxFps; }
    bool boxLightSupported() const { return true; }
    int boxMode() const { return m_box.mode; }
    bool boxRainbow() const { return m_box.colorful; }
    int boxBrightness() const { return m_box.brightness; }
    int boxSpeed() const { return m_box.speed; }
    QColor boxColor() const { return QColor(m_box.r, m_box.g, m_box.b); }

    // Travel
    Q_INVOKABLE void setActuation(const QVariantList &indices, double mm);
    Q_INVOKABLE void setRapidTrigger(const QVariantList &indices, bool on, double pressMm, double releaseMm);
    Q_INVOKABLE void setDeadZones(double topMm, double bottomMm);
    // Keymap
    Q_INVOKABLE void setKeycode(int layer, int index, int keycode);
    Q_INVOKABLE void resetKeymap();
    Q_INVOKABLE QString keycodeLabel(int keycode) const;
    Q_INVOKABLE QString keycodeShort(int keycode) const;
    Q_INVOKABLE QVariantList keycodeGroups() const;    // [{title, keys:[{code,name}]}]
    Q_INVOKABLE QString keyName(int index) const;       // from layer 0 keymap
    // Features
    Q_INVOKABLE void setWasdSwap(bool on);
    Q_INVOKABLE void setMacMode(bool on);
    Q_INVOKABLE void setWinLock(bool on);
    Q_INVOKABLE void setNkro(bool on);
    Q_INVOKABLE void setBottomOptimize(bool on);
    Q_INVOKABLE void setGameMode(bool on);
    // Lighting
    Q_INVOKABLE void setLighting(int effect, int speed, int brightness, const QColor &color);
    Q_INVOKABLE void setKeyColor(const QVariantList &indices, const QColor &color);
    Q_INVOKABLE void setBoxLight(int mode, bool rainbow, int brightness, int speed, const QColor &color);
    // Flux live lighting
    Q_INVOKABLE void startFluxEffect(const QString &id);
    Q_INVOKABLE void stopFluxEffect();
    Q_INVOKABLE void setFluxSpeed(double s);
    Q_INVOKABLE void setFluxIntensity(double i);
    Q_INVOKABLE void setFluxPaletteColor(int index, const QColor &c);
    Q_INVOKABLE void resetFluxPalette();
    Q_INVOKABLE void bakeFluxFrame();
    // Advanced keys
    Q_INVOKABLE void loadAdvanced();
    Q_INVOKABLE void setSocd(int slot, const QVariantMap &s);
    Q_INVOKABLE void setDks(int slot, const QVariantList &actions);
    Q_INVOKABLE void setTapDance(int slot, int tap, int hold, int termMs);
    // Macros
    Q_INVOKABLE void saveMacro(int index, const QString &name, const QVariantList &events);
    Q_INVOKABLE void deleteMacro(int index);
    Q_INVOKABLE QString macroName(int index) const;
    // Calibration / reset
    Q_INVOKABLE void startCalibration();
    Q_INVOKABLE void finishCalibration();
    Q_INVOKABLE void factoryReset();
    // Presets
    Q_INVOKABLE QVariantList indicesForKeycodes(const QVariantList &codes) const;
    Q_INVOKABLE QString applyOsuPreset(const QString &variant);   // "std" | "mania4" | "mania7"
    bool osuMode() const { return m_osuMode; }
    void setOsuMode(bool on);
    QVariantList osuKeys() const;
    QString osuKeysText() const;
    Q_INVOKABLE void setOsuKeys(const QVariantList &indices);
    Q_INVOKABLE void setOsuKeySet(const QString &variant);        // "std" | "mania4" | "mania7"

signals:
    void travelSettingsChanged();
    void keymapChanged();
    void featuresChanged();
    void lightingChanged();
    void advancedChanged();
    void macrosChanged();
    void liveTravelChanged();
    void travelChanged();
    void calibratingChanged();
    void fluxChanged();
    void osuChanged();
    void liveColorsChanged();

private:
    struct Snapshot {
        std::string version;
        std::vector<uint16_t> act;
        std::vector<mad::RtInfo> rt;
        mad::DeadZones dead;
        std::vector<uint16_t> keymap;
        mad::Features features;
        bool bottom = false, game = false;
        int defaultLayer = 0;
        mad::Lighting light;
        std::vector<mad::Rgb> colors;
        mad::BoxLight box;
        int macroCount = 0, macroSize = 0;
        std::vector<uint8_t> macroBuf;
        bool ok = false;
    };

    void loadData();
    void pushFluxParams();
    void saveFluxSettings() const;
    void streamFrame(const std::vector<fx::Color> &frame);   // I/O thread
    void restoreAfterFlux(const std::vector<mad::Rgb> &colors);  // I/O thread
    void ioIdle();
    void applySnapshot(const Snapshot &s);
    void write(std::function<bool(mad::Protocol &)> fn, const QString &what);
    void writeFeatures();
    void writeMacroBuffer();
    std::vector<int> toIndices(const QVariantList &l) const;

    static std::vector<QVariantList> parseMacros(const std::vector<uint8_t> &buf, int count);
    static std::vector<uint8_t> encodeMacro(const QVariantList &events);

    HidInfo m_info;
    HidDevice m_dev;  // I/O thread only
    std::unique_ptr<mad::Protocol> m_proto;

    QVariantList m_layout;
    QVariantList m_effects;
    QVariantList m_keycodeGroups;
    QHash<int, QString> m_extraNames;
    double m_travelMax = 3.5, m_actMin = 0.10, m_actMax = 3.50, m_rtMin = 0.02, m_rtMax = 3.50, m_dzMax = 1.00;
    int m_speedMax = 255, m_brightnessMax = 200;

    std::vector<uint16_t> m_act = std::vector<uint16_t>(mad::kKeys, 150);
    std::vector<mad::RtInfo> m_rt = std::vector<mad::RtInfo>(mad::kKeys);
    mad::DeadZones m_dead;
    std::vector<uint16_t> m_keymap = std::vector<uint16_t>(mad::kLayers * mad::kKeys);
    mad::Features m_features;
    bool m_bottomOptimize = false, m_gameMode = false;
    int m_defaultLayer = 0;
    mad::Lighting m_light;
    std::vector<mad::Rgb> m_colors = std::vector<mad::Rgb>(mad::kKeys);
    std::vector<mad::SocdSlot> m_socd;
    std::vector<std::array<mad::DksAction, 4>> m_dks;
    std::vector<mad::TapDance> m_td;
    bool m_advancedLoaded = false;
    int m_macroCount = 0, m_macroBufSize = 0;
    std::vector<QVariantList> m_macros;

    std::atomic<bool> m_liveIo{false};
    bool m_live = false;
    QVariantList m_travel;
    double m_travelFps = 0;
    QElapsedTimer m_fpsClock;
    int m_fpsFrames = 0;
    bool m_calibrating = false;

    // Flux live lighting
    fx::Engine m_engine;
    QString m_fxId;
    double m_fxSpeed = 1.0;
    double m_fxIntensity = 0.85;
    std::array<QColor, 3> m_fxPalette;
    QVariantList m_liveColors;
    double m_fxFps = 0;
    std::atomic<bool> m_fxIo{false};
    mad::Lighting m_lightBeforeFx;          // GUI copy, restored on stop
    std::vector<mad::Rgb> m_lastSent;       // I/O thread
    std::vector<float> m_smooth;            // I/O thread: temporally smoothed frame (r,g,b per key)
    std::vector<int> m_ledSource;           // matrix index whose colour an empty slot copies
    // The keyboard acknowledges an LED write in ~0.25 ms and redraws its LED
    // chain *afterwards* (~2-3 ms). A packet arriving during that redraw
    // garbles the first LEDs of the chain (the left side), so every LED write
    // is followed by a quiet gap before the next USB packet.
    std::atomic<int> m_ledGapMs{5};
    std::vector<mad::Rgb> m_lastFrame;      // GUI copy (for baking)
    std::chrono::steady_clock::time_point m_fxEpoch = std::chrono::steady_clock::now();
    int m_fxFrames = 0;
    QElapsedTimer m_fxClock;
    mad::BoxLight m_box;
    AudioTap m_audioTap;
    void updateAudioTap();
    bool m_osuMode = false;
    QList<int> m_osuKeys;   // empty = default A + D
};

} // namespace flux
