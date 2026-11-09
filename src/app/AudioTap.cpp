#include "AudioTap.h"

#include <QDateTime>
#include <QLoggingCategory>

#include <cmath>
#include <complex>
#include <cstring>

Q_LOGGING_CATEGORY(lcAudio, "flux.audio")

namespace flux {

namespace {
constexpr int kRate = 24000;
constexpr int kFft = 1024;
constexpr int kRing = 4096;

void fft(std::vector<std::complex<float>> &a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const float ang = -2.f * float(M_PI) / float(len);
        const std::complex<float> wl(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<float> w(1.f, 0.f);
            for (size_t k = 0; k < len / 2; ++k) {
                const auto u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}
} // namespace

AudioTap::AudioTap(QObject *parent)
    : QObject(parent)
    , m_ring(kRing, 0.f)
{
    m_gain.fill(0.02f);
    connect(&m_proc, &QProcess::readyReadStandardOutput, this, &AudioTap::onData);
    connect(&m_proc, &QProcess::finished, this, [this] {
        // Sink changed (e.g. headphones reconnected): restart quietly.
        if (m_wanted)
            QTimer::singleShot(1500, this, [this] { if (m_wanted && !running()) start(); });
    });
    m_tick.setInterval(20);
    connect(&m_tick, &QTimer::timeout, this, &AudioTap::analyse);
}

AudioTap::~AudioTap()
{
    stop();
}

void AudioTap::start()
{
    m_wanted = true;
    if (running())
        return;
    m_proc.setProcessChannelMode(QProcess::SeparateChannels);
    m_proc.start(QStringLiteral("parec"),
                 {QStringLiteral("--device=@DEFAULT_MONITOR@"), QStringLiteral("--format=float32le"),
                  QStringLiteral("--rate=%1").arg(kRate), QStringLiteral("--channels=1"), QStringLiteral("--latency-msec=15"),
                  QStringLiteral("--client-name=Flux"), QStringLiteral("--stream-name=Keyboard lighting")});
    if (!m_proc.waitForStarted(1000))
        qCWarning(lcAudio) << "couldn't start parec:" << m_proc.errorString();
    m_tick.start();
}

void AudioTap::stop()
{
    m_wanted = false;
    m_tick.stop();
    if (running()) {
        m_proc.terminate();
        if (!m_proc.waitForFinished(500))
            m_proc.kill();
    }
}

void AudioTap::onData()
{
    m_pending += m_proc.readAllStandardOutput();
    const int n = int(m_pending.size() / 4);
    const float *f = reinterpret_cast<const float *>(m_pending.constData());
    for (int i = 0; i < n; ++i) {
        m_ring[m_head] = f[i];
        m_head = (m_head + 1) % kRing;
    }
    m_pending.remove(0, n * 4);
}

void AudioTap::analyse()
{
    Frame fr;
    fr.active = running();

    // Latest kFft samples, Hann windowed.
    std::vector<std::complex<float>> buf(kFft);
    float sumSq = 0;
    for (int i = 0; i < kFft; ++i) {
        const float s = m_ring[(m_head + kRing - kFft + i) % kRing];
        sumSq += s * s;
        const float w = 0.5f - 0.5f * std::cos(2.f * float(M_PI) * i / (kFft - 1));
        buf[i] = {s * w, 0.f};
    }
    const float rms = std::sqrt(sumSq / kFft);
    fft(buf);

    // 14 log-spaced bands, 40 Hz .. 10 kHz, each with its own slow auto-gain
    // so quiet and loud music both fill the board.
    const float fLo = 40.f, fHi = 10000.f;
    for (int b = 0; b < 14; ++b) {
        const float f0 = fLo * std::pow(fHi / fLo, b / 14.f), f1 = fLo * std::pow(fHi / fLo, (b + 1) / 14.f);
        const int k0 = std::max(1, int(f0 * kFft / kRate)), k1 = std::max(k0 + 1, int(f1 * kFft / kRate));
        float e = 0;
        for (int k = k0; k < k1 && k < kFft / 2; ++k)
            e = std::max(e, std::abs(buf[k]));
        m_gain[b] = std::max(m_gain[b] * 0.997f, e);
        const float norm = m_gain[b] > 1e-4f ? e / m_gain[b] : 0.f;
        fr.bands[b] = std::clamp(std::pow(norm, 1.4f), 0.f, 1.f);
    }

    // Loudness with a slow auto-gain; silence stays dark.
    m_peakLevel = std::max(m_peakLevel * 0.998f, rms);
    fr.level = rms < 1e-4f ? 0.f : std::clamp(rms / std::max(0.02f, m_peakLevel), 0.f, 1.f);
    fr.bass = (fr.bands[0] + fr.bands[1] + fr.bands[2]) / 3.f * fr.level;
    fr.treble = (fr.bands[10] + fr.bands[11] + fr.bands[12] + fr.bands[13]) / 4.f * fr.level;

    // Waveform: the last ~23 ms, so a few cycles of a bass note are visible.
    float wpk = 1e-4f;
    for (int i = 0; i < 28; ++i) {
        float acc = 0;
        for (int k = 0; k < 20; ++k)
            acc += m_ring[(m_head + kRing - 560 + i * 20 + k) % kRing];
        fr.wave[i] = acc / 20.f;
        wpk = std::max(wpk, std::abs(fr.wave[i]));
    }
    m_wavePeak = std::max(m_wavePeak * 0.99f, wpk);
    for (auto &w : fr.wave)
        w = std::clamp(w / std::max(0.01f, m_wavePeak), -1.f, 1.f) * std::min(1.f, fr.level * 1.5f);

    // Beat: bass well above its recent average, at most ~4 per second.
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (fr.bass > 0.25f && fr.bass > m_bassAvg * 1.45f && now - m_lastBeatMs > 230) {
        ++m_beats;
        m_lastBeatMs = now;
    }
    m_bassAvg = m_bassAvg * 0.93f + fr.bass * 0.07f;
    fr.beats = m_beats;

    if (m_sink)
        m_sink(fr);
}

} // namespace flux
