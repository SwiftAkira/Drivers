#pragma once

#include <QObject>
#include <QProcess>
#include <QTimer>

#include <array>
#include <functional>
#include <vector>

namespace flux {

// Captures what the PC is playing (default sink monitor via parec) and turns
// it into a small analysis frame for lighting: 14 log-spaced bands, a short
// waveform, loudness, bass energy and a beat counter.
class AudioTap : public QObject {
    Q_OBJECT
public:
    struct Frame {
        std::array<float, 14> bands{};
        std::array<float, 28> wave{};
        float level = 0, bass = 0, treble = 0;
        uint32_t beats = 0;
        bool active = false;
    };

    explicit AudioTap(QObject *parent = nullptr);
    ~AudioTap() override;

    void start();
    void stop();
    bool running() const { return m_proc.state() != QProcess::NotRunning; }
    void setSink(std::function<void(const Frame &)> sink) { m_sink = std::move(sink); }

private:
    void onData();
    void analyse();

    QProcess m_proc;
    QTimer m_tick;
    std::vector<float> m_ring;
    size_t m_head = 0;
    QByteArray m_pending;
    std::function<void(const Frame &)> m_sink;

    std::array<float, 14> m_gain{};
    float m_peakLevel = 0.05f, m_wavePeak = 0.05f;
    float m_bassAvg = 0;
    uint32_t m_beats = 0;
    qint64 m_lastBeatMs = 0;
    bool m_wanted = false;
};

} // namespace flux
