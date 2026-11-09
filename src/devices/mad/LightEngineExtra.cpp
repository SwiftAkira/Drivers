// Signature effects: physics sims, magnetic fields, lightning, audio.
#include "LightEngine.h"

#include <algorithm>
#include <cmath>

namespace flux::fx {

namespace {

constexpr float kPi = 3.14159265f;

Color mixc(const Color &a, const Color &b, float t)
{
    t = std::clamp(t, 0.f, 1.f);
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}
Color sc(const Color &c, float k) { return {c.r * k, c.g * k, c.b * k}; }
Color addc(const Color &a, const Color &b) { return {a.r + b.r, a.g + b.g, a.b + b.b}; }
float segDist(float px, float py, float ax, float ay, float bx, float by)
{
    const float vx = bx - ax, vy = by - ay;
    const float l2 = vx * vx + vy * vy;
    float t = l2 > 0 ? ((px - ax) * vx + (py - ay) * vy) / l2 : 0;
    t = std::clamp(t, 0.f, 1.f);
    const float dx = px - (ax + t * vx), dy = py - (ay + t * vy);
    return std::sqrt(dx * dx + dy * dy);
}

} // namespace

void Engine::setAudio(const AudioFrame &a)
{
    std::lock_guard lock(m_mutex);
    m_audio = a;
}

bool Engine::needsAudio() const
{
    std::lock_guard lock(m_mutex);
    for (const auto &e : effects())
        if (m_params.effect == e.id)
            return e.usesAudio;
    return false;
}

float Engine::gridSample(const std::vector<float> &g, float x, float y) const
{
    if (g.empty())
        return 0;
    x = std::clamp(x, 0.f, float(m_gw - 1));
    y = std::clamp(y, 0.f, float(m_gh - 1));
    const int x0 = int(x), y0 = int(y);
    const int x1 = std::min(x0 + 1, m_gw - 1), y1 = std::min(y0 + 1, m_gh - 1);
    const float fx = x - x0, fy = y - y0;
    const float a = g[y0 * m_gw + x0], b = g[y0 * m_gw + x1], c = g[y1 * m_gw + x0], d = g[y1 * m_gw + x1];
    return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy;
}

void Engine::updateExtra(const std::string &fx, double tSec, float dt, float sp, const std::vector<bool> &pressed,
                         const std::vector<float> &depth, const std::vector<float> &travel)
{
    const size_t n = m_keys.size();
    std::uniform_real_distribution<float> uni(0.f, 1.f);
    if (m_extraFor != fx) {
        m_extraFor = fx;
        m_gw = std::max(4, int(std::ceil(m_w * 2)));
        m_gh = std::max(4, int(std::ceil(m_h * 2)));
        m_simA.assign(size_t(m_gw * m_gh), 0.f);
        m_simB.assign(size_t(m_gw * m_gh), 0.f);
        m_dyeR = m_dyeG = m_dyeB = m_simA;
        m_poles.clear();
        m_stars.clear();
        m_bolts.clear();
        m_alive.assign(n, 0);
        m_age.assign(n, 0.f);
        m_taps.clear();
        m_beatPeriod = 0;
        m_lastBeatIdx = -1;
        m_lastBeats = m_audio.beats;
        m_peaks.fill(0.f);
        m_lifeT = tSec;
        m_lifeQuiet = tSec;
    }
    auto nextColor = [&] { return m_params.palette[(m_colorIdx++) % 3]; };

    if (fx == "water") {
        // Classic 2D ripple tank: discrete wave equation with damping.
        auto stone = [&](float kx, float ky, float amp) {
            const int gx = std::clamp(int(kx * 2), 1, m_gw - 2), gy = std::clamp(int(ky * 2), 1, m_gh - 2);
            m_simA[gy * m_gw + gx] -= amp;
            m_simA[gy * m_gw + gx + 1] -= amp * 0.5f;
            m_simA[gy * m_gw + gx - 1] -= amp * 0.5f;
        };
        for (size_t i = 0; i < n; ++i)
            if (pressed[i])
                stone(m_keys[i].x, m_keys[i].y, 3.0f + 4.0f * depth[i]);
        if (uni(m_rng) < dt * 0.8f * sp) // light ambient rain
            stone(uni(m_rng) * m_w, uni(m_rng) * m_h, 0.8f);
        const int steps = std::max(1, int(dt * 60 * sp + 0.5f));
        for (int s = 0; s < steps; ++s) {
            for (int y = 1; y < m_gh - 1; ++y)
                for (int x = 1; x < m_gw - 1; ++x) {
                    const int k = y * m_gw + x;
                    const float v = (m_simA[k - 1] + m_simA[k + 1] + m_simA[k - m_gw] + m_simA[k + m_gw]) * 0.5f - m_simB[k];
                    m_simB[k] = v * 0.975f;
                }
            std::swap(m_simA, m_simB);
        }
    } else if (fx == "ink") {
        // Dye advected through a curl-noise flow (divergence free → swirls).
        for (size_t i = 0; i < n; ++i) {
            if (!pressed[i])
                continue;
            const Color c = nextColor();
            const float kx = m_keys[i].x * 2, ky = m_keys[i].y * 2;
            for (int y = 0; y < m_gh; ++y)
                for (int x = 0; x < m_gw; ++x) {
                    const float d2 = (x - kx) * (x - kx) + (y - ky) * (y - ky);
                    const float w = std::exp(-d2 / (2.2f + 3.f * depth[i])) * 1.4f;
                    const int k = y * m_gw + x;
                    m_dyeR[k] += c.r * w;
                    m_dyeG[k] += c.g * w;
                    m_dyeB[k] += c.b * w;
                }
        }
        if (uni(m_rng) < dt * 0.35f) { // a faint ambient wisp so it's never empty
            const Color c = m_params.palette[size_t(uni(m_rng) * 3) % 3];
            const int k = int(uni(m_rng) * m_gh) * m_gw + int(uni(m_rng) * m_gw);
            m_dyeR[k] += c.r * 0.8f;
            m_dyeG[k] += c.g * 0.8f;
            m_dyeB[k] += c.b * 0.8f;
        }
        const float t = float(tSec) * sp;
        auto psi = [&](float x, float y) { return std::sin(x * 0.35f + t * 0.6f) * std::cos(y * 0.55f - t * 0.4f) + 0.6f * std::sin((x + y) * 0.22f - t * 0.3f); };
        std::vector<float> nr(m_dyeR.size()), ng(nr.size()), nb(nr.size());
        const float adv = dt * 7.f * sp, e = 0.5f;
        for (int y = 0; y < m_gh; ++y)
            for (int x = 0; x < m_gw; ++x) {
                const float vx = (psi(float(x), y + e) - psi(float(x), y - e)) / (2 * e);
                const float vy = -(psi(x + e, float(y)) - psi(x - e, float(y))) / (2 * e);
                const float sx = x - vx * adv, sy = y - vy * adv;
                const int k = y * m_gw + x;
                const float decay = std::pow(0.55f, dt);
                nr[k] = gridSample(m_dyeR, sx, sy) * decay;
                ng[k] = gridSample(m_dyeG, sx, sy) * decay;
                nb[k] = gridSample(m_dyeB, sx, sy) * decay;
            }
        m_dyeR.swap(nr);
        m_dyeG.swap(ng);
        m_dyeB.swap(nb);
    } else if (fx == "magnet") {
        for (size_t i = 0; i < n; ++i)
            if (pressed[i]) {
                const float q = (m_poles.size() % 2 == 0) ? 1.f : -1.f;
                m_poles.push_back({m_keys[i].x, m_keys[i].y, q * (0.7f + 0.6f * depth[i]), float(tSec)});
            }
        m_poles.erase(std::remove_if(m_poles.begin(), m_poles.end(), [&](const Pole &p) { return tSec - p.t0 > 5.0 / sp; }),
                      m_poles.end());
        if (m_poles.size() > 4)
            m_poles.erase(m_poles.begin(), m_poles.end() - 4);
    } else if (fx == "constellation") {
        for (size_t i = 0; i < n; ++i)
            if (pressed[i])
                m_stars.push_back({m_keys[i].x, m_keys[i].y, float(tSec), nextColor()});
        m_stars.erase(std::remove_if(m_stars.begin(), m_stars.end(), [&](const Star &s) { return tSec - s.t0 > 7.0 / sp; }),
                      m_stars.end());
        if (m_stars.size() > 14)
            m_stars.erase(m_stars.begin(), m_stars.end() - 14);
    } else if (fx == "lightning") {
        auto bolt = [&](float ax, float ay, float bx, float by, Color c) {
            // Midpoint displacement gives a convincing jagged arc.
            std::vector<std::array<float, 2>> pts{{ax, ay}, {bx, by}};
            float jitter = 0.9f;
            for (int lvl = 0; lvl < 4; ++lvl) {
                std::vector<std::array<float, 2>> nx;
                for (size_t k = 0; k + 1 < pts.size(); ++k) {
                    const auto &p = pts[k], &q = pts[k + 1];
                    const float mx = (p[0] + q[0]) / 2, my = (p[1] + q[1]) / 2;
                    const float dx = q[0] - p[0], dy = q[1] - p[1];
                    const float off = (uni(m_rng) - 0.5f) * jitter;
                    nx.push_back(p);
                    nx.push_back({mx - dy * off * 0.35f, my + dx * off * 0.35f});
                }
                nx.push_back(pts.back());
                pts.swap(nx);
                jitter *= 0.6f;
            }
            m_bolts.push_back({pts, float(tSec), c});
        };
        for (size_t i = 0; i < n; ++i) {
            if (!pressed[i])
                continue;
            // Strike toward a random spot a few keys away.
            const float ang = uni(m_rng) * 2 * kPi, dist = 3.f + uni(m_rng) * 5.f;
            const float bx = std::clamp(m_keys[i].x + std::cos(ang) * dist, 0.f, m_w), by = std::clamp(m_keys[i].y + std::sin(ang) * dist * 0.5f, 0.f, m_h);
            bolt(m_keys[i].x, m_keys[i].y, bx, by, nextColor());
        }
        // Tesla arc between two held keys.
        std::vector<size_t> held;
        for (size_t i = 0; i < n && i < travel.size(); ++i)
            if (m_keys[i].present && travel[i] > 1.0f)
                held.push_back(i);
        if (held.size() >= 2 && tSec - m_lastArc > 0.12 / sp) {
            m_lastArc = tSec;
            const size_t a = held[size_t(uni(m_rng) * held.size()) % held.size()];
            size_t b = held[size_t(uni(m_rng) * held.size()) % held.size()];
            if (a == b)
                b = held[(std::find(held.begin(), held.end(), a) - held.begin() + 1) % held.size()];
            bolt(m_keys[a].x, m_keys[a].y, m_keys[b].x, m_keys[b].y, m_params.palette[2]);
        }
        m_bolts.erase(std::remove_if(m_bolts.begin(), m_bolts.end(), [&](const Bolt &b) { return tSec - b.t0 > 0.6 / sp; }),
                      m_bolts.end());
        if (m_bolts.size() > 10)
            m_bolts.erase(m_bolts.begin(), m_bolts.end() - 10);
    } else if (fx == "life") {
        for (size_t i = 0; i < n; ++i)
            if (pressed[i]) {
                m_alive[i] = 1;
                m_age[i] = 0;
                for (auto [j, w] : m_neighbours[i])
                    if (uni(m_rng) < 0.45f)
                        m_alive[j] = 1;
                m_lifeQuiet = tSec;
            }
        if (tSec - m_lifeT > 0.28 / sp) {
            m_lifeT = tSec;
            std::vector<uint8_t> next(n, 0);
            int pop = 0;
            for (size_t i = 0; i < n; ++i) {
                if (!m_keys[i].present)
                    continue;
                int nb = 0;
                for (auto [j, w] : m_neighbours[i])
                    nb += m_alive[j];
                next[i] = (m_alive[i] && (nb == 2 || nb == 3)) || (!m_alive[i] && nb == 3);
                pop += next[i];
            }
            for (size_t i = 0; i < n; ++i)
                m_age[i] = next[i] ? (m_alive[i] ? m_age[i] + 1 : 0) : m_age[i];
            m_alive = next;
            if (pop < 3 && tSec - m_lifeQuiet > 2.5) { // reseed a dead world
                for (size_t i = 0; i < n; ++i)
                    if (m_keys[i].present && uni(m_rng) < 0.3f)
                        m_alive[i] = 1, m_age[i] = 0;
                m_lifeQuiet = tSec;
            }
        }
    } else if (fx == "tempo") {
        bool any = false;
        for (size_t i = 0; i < n; ++i)
            any |= pressed[i];
        if (any) {
            if (!m_taps.empty()) {
                const double iv = tSec - m_taps.back();
                if (iv > 1.5)
                    m_taps.clear();
            }
            m_taps.push_back(tSec);
            if (m_taps.size() > 9)
                m_taps.erase(m_taps.begin());
            if (m_taps.size() >= 3) {
                std::vector<double> iv;
                for (size_t k = 1; k < m_taps.size(); ++k)
                    iv.push_back(m_taps[k] - m_taps[k - 1]);
                std::nth_element(iv.begin(), iv.begin() + iv.size() / 2, iv.end());
                const double med = iv[iv.size() / 2];
                if (med > 0.07 && med < 1.2) {
                    m_beatPeriod = med;
                    m_beatT0 = tSec;
                }
            }
        }
    } else if (fx == "echo") {
        for (size_t i = 0; i < n; ++i) {
            if (!pressed[i])
                continue;
            const Color c = nextColor();
            for (int k = 0; k < 4; ++k) {
                const bool mirror = k % 2;
                m_ripples.push_back({mirror ? m_w - m_keys[i].x : m_keys[i].x, m_keys[i].y, float(tSec + k * 0.3 / sp),
                                     (0.6f + 0.6f * depth[i]) * std::pow(0.6f, float(k)), mixc(c, m_params.palette[(m_colorIdx + k) % 3], 0.3f * k)});
            }
        }
        m_ripples.erase(std::remove_if(m_ripples.begin(), m_ripples.end(), [&](const Ripple &r) { return tSec - r.t0 > 1.4 / sp; }),
                        m_ripples.end());
        if (m_ripples.size() > 40)
            m_ripples.erase(m_ripples.begin(), m_ripples.end() - 40);
    } else if (fx == "bassdrop") {
        if (m_audio.beats != m_lastBeats) {
            m_lastBeats = m_audio.beats;
            m_ripples.push_back({m_w / 2, m_h - 0.5f, float(tSec), 0.5f + m_audio.bass, nextColor()});
        }
        m_ripples.erase(std::remove_if(m_ripples.begin(), m_ripples.end(), [&](const Ripple &r) { return tSec - r.t0 > 1.2; }),
                        m_ripples.end());
    } else if (fx == "spectrum") {
        for (int b = 0; b < 14; ++b)
            m_peaks[b] = std::max(m_audio.bands[b], m_peaks[b] - dt * 0.6f);
    } else if (fx == "soundwave") {
        for (auto &g : m_glow)
            g *= std::pow(0.0005f, dt);
    }
}

bool Engine::renderExtra(const std::string &fx, size_t i, float x, float y, float t, double tSec, Color &c)
{
    const auto &P = m_params.palette;
    if (fx == "water") {
        const float h = gridSample(m_simA, x * 2, y * 2);
        const float up = std::clamp(h * 0.9f, -1.f, 1.f);
        c = addc(sc(P[0], 0.18f), up > 0 ? sc(P[1], std::sqrt(up)) : sc(P[2], std::sqrt(-up) * 0.8f));
        c = addc(c, sc({1, 1, 1}, std::max(0.f, up - 0.55f) * 1.6f));
        return true;
    }
    if (fx == "ink") {
        c = {gridSample(m_dyeR, x * 2, y * 2), gridSample(m_dyeG, x * 2, y * 2), gridSample(m_dyeB, x * 2, y * 2)};
        return true;
    }
    if (fx == "magnet") {
        std::vector<Pole> poles = m_poles;
        if (poles.empty()) { // ambient: two poles slowly orbiting the centre
            const float a = t * 0.35f;
            poles.push_back({m_w / 2 + std::cos(a) * m_w * 0.28f, m_h / 2 + std::sin(a) * m_h * 0.3f, 1.f, 0});
            poles.push_back({m_w / 2 - std::cos(a) * m_w * 0.28f, m_h / 2 - std::sin(a) * m_h * 0.3f, -1.f, 0});
        }
        float psi = 0, bx = 0, by = 0, nearest = 99;
        float sign = 1;
        for (const auto &p : poles) {
            const float fade = p.t0 > 0 ? std::max(0.f, 1.f - float(tSec - p.t0) * m_params.speed / 5.f) : 1.f;
            const float dx = x - p.x, dy = (y - p.y) * 1.6f;
            const float r2 = dx * dx + dy * dy + 0.15f;
            psi += p.q * fade * std::atan2(dy, dx);
            bx += p.q * fade * dx / r2;
            by += p.q * fade * dy / r2;
            if (r2 < nearest) {
                nearest = r2;
                sign = p.q;
            }
        }
        const float B = std::sqrt(bx * bx + by * by);
        const float lines = std::pow(0.5f + 0.5f * std::cos(psi * 2.f - t * 3.f), 6.f);
        const Color pole = sign > 0 ? P[0] : P[1];
        c = addc(sc(pole, std::clamp(B * 0.45f, 0.f, 1.f) * 0.55f + 0.04f), sc(P[2], lines * std::clamp(B * 0.8f, 0.f, 1.f)));
        c = addc(c, sc({1, 1, 1}, std::max(0.f, 0.5f - nearest) * 1.6f));
        return true;
    }
    if (fx == "constellation") {
        // Faint background sky
        const float tw = 0.5f + 0.5f * std::sin(t * (1.3f + std::fmod(i * 0.37f, 1.f) * 2.f) + i * 1.7f);
        c = sc(P[1], 0.03f + 0.04f * tw);
        for (size_t s = 0; s < m_stars.size(); ++s) {
            const auto &st = m_stars[s];
            const float age = float(tSec - st.t0) * m_params.speed;
            const float life = std::max(0.f, 1.f - age / 7.f);
            const float dx = x - st.x, dy = (y - st.y) * 1.1f;
            const float d2 = dx * dx + dy * dy;
            c = addc(c, sc(mixc(st.c, {1, 1, 1}, 0.4f), std::exp(-d2 / 0.18f) * life * (0.8f + 0.2f * std::sin(age * 9.f))));
            if (s + 1 < m_stars.size()) {
                const auto &nx = m_stars[s + 1];
                const float d = segDist(x, y, st.x, st.y, nx.x, nx.y);
                const float draw = std::clamp((float(tSec) - nx.t0) * 6.f, 0.f, 1.f); // line draws in quickly
                const float l2 = std::max(0.f, 1.f - float(tSec - nx.t0) * m_params.speed / 7.f);
                c = addc(c, sc(mixc(st.c, nx.c, 0.5f), std::exp(-d * d / 0.08f) * 0.55f * draw * std::min(life, l2)));
            }
        }
        return true;
    }
    if (fx == "lightning") {
        c = sc(P[0], 0.025f);
        for (const auto &b : m_bolts) {
            const float age = float(tSec) - b.t0;
            // Bright strike, a quick re-strike flicker, then a violet afterglow.
            const float flash = (age < 0.06f ? 1.f : age < 0.1f ? 0.3f : age < 0.16f ? 0.9f : std::exp(-(age - 0.16f) * 7.f) * 0.7f);
            float d = 99;
            for (size_t k = 0; k + 1 < b.pts.size(); ++k)
                d = std::min(d, segDist(x, y, b.pts[k][0], b.pts[k][1], b.pts[k + 1][0], b.pts[k + 1][1]));
            const float core = std::exp(-d * d / 0.10f), glow = std::exp(-d * d / 0.9f);
            c = addc(c, sc(mixc(b.c, {1, 1, 1}, core * 0.7f), (core + glow * 0.45f) * flash));
        }
        return true;
    }
    if (fx == "life") {
        const float age = m_age[i];
        if (m_alive[i])
            c = mixc(P[0], mixc(P[1], P[2], std::clamp((age - 3) / 6.f, 0.f, 1.f)), std::clamp(age / 3.f, 0.f, 1.f));
        else
            c = sc(P[0], 0.03f);
        return true;
    }
    if (fx == "tempo") {
        if (m_beatPeriod <= 0 || tSec - m_beatT0 > 3.0) {
            const float b = 0.5f - 0.5f * std::cos(t * 0.8f);
            c = sc(P[0], 0.05f + 0.25f * b * b);
            return true;
        }
        const double since = tSec - m_beatT0;
        const long idx = long(std::floor(since / m_beatPeriod + 0.5));
        const float phase = float(since - idx * m_beatPeriod); // seconds from the nearest beat
        const float pulse = std::exp(-std::abs(phase) / float(m_beatPeriod * 0.22));
        const Color col = P[size_t(std::max(0L, idx)) % 3];
        const float dx = x - m_w / 2, dy = (y - m_h / 2) * 1.6f;
        const float ring = std::exp(-std::pow(std::sqrt(dx * dx + dy * dy) - std::max(0.f, phase) * 30.f, 2.f) / 1.4f);
        c = addc(sc(col, 0.12f + 0.75f * pulse), sc({1, 1, 1}, ring * 0.5f * (phase > 0)));
        return true;
    }
    if (fx == "echo" || fx == "bassdrop") {
        c = fx == "bassdrop" ? sc(P[0], 0.05f + 0.25f * m_audio.level) : sc(P[0], 0.04f);
        for (const auto &r : m_ripples) {
            const float age = float(tSec) - r.t0;
            if (age < 0)
                continue;
            const float radius = age * (fx == "bassdrop" ? 16.f : 9.f) * m_params.speed;
            const float dx = x - r.x, dy = (y - r.y) * 1.15f;
            const float dist = std::sqrt(dx * dx + dy * dy);
            const float ring = std::exp(-std::pow((dist - radius) / (0.7f + 0.4f * r.strength), 2.f));
            const float fade = std::max(0.f, 1.f - age / (fx == "bassdrop" ? 1.2f : 1.1f));
            c = addc(c, sc(r.c, ring * fade * r.strength));
        }
        return true;
    }
    if (fx == "spectrum") {
        const int band = std::clamp(int(x / m_w * 14), 0, 13);
        const float fromBottom = std::clamp((m_h - y) / m_h, 0.f, 1.f); // 0.1 .. 0.9 for the 5 rows
        const float level = m_audio.bands[band];
        const float lit = std::clamp((level * 1.1f - fromBottom + 0.12f) * 5.f, 0.f, 1.f);
        c = addc(sc(P[0], 0.04f), sc(mixc(P[0], mixc(P[1], P[2], (fromBottom - 0.5f) * 2), fromBottom * 1.6f), lit));
        const float peakRow = m_peaks[band];
        if (std::abs(fromBottom - peakRow) < 0.11f && peakRow > 0.12f)
            c = addc(c, sc({1, 1, 1}, 0.6f));
        return true;
    }
    if (fx == "soundwave") {
        const float u = std::clamp(x / m_w, 0.f, 1.f) * 27.f;
        const int i0 = int(u), i1 = std::min(27, i0 + 1);
        const float v = m_audio.wave[i0] + (m_audio.wave[i1] - m_audio.wave[i0]) * (u - i0);
        const float target = m_h / 2 - v * m_h * 0.48f;
        const float d = (y - target);
        const float line = std::exp(-d * d / 0.35f);
        m_glow[i] = std::max(m_glow[i], line);
        c = addc(sc(pal(u / 27.f * 0.7f + t * 0.03f), m_glow[i]), sc({1, 1, 1}, line * line * 0.35f * m_audio.level));
        c = addc(c, sc(P[0], 0.03f));
        return true;
    }
    if (fx == "aura") {
        const float lv = m_audio.level;
        const float tt = t * (0.6f + lv * 2.5f);
        float v = std::sin(x * 0.55f + tt) + std::sin(y * 0.9f + tt * 0.7f) + std::sin(x * 0.4f + y * 0.6f + tt * 1.1f);
        v += m_audio.bass * 2.f * std::sin(std::sqrt((x - m_w / 2) * (x - m_w / 2) + (y - m_h / 2) * (y - m_h / 2) * 3.f) * 1.2f - tt * 2.f);
        c = sc(pal(v / 8.f + 0.5f + m_audio.treble * 0.3f), 0.12f + 0.88f * std::clamp(lv * 1.2f, 0.f, 1.f));
        return true;
    }
    return false;
}

} // namespace flux::fx
