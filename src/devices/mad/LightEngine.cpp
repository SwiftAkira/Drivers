#include "LightEngine.h"

#include <algorithm>
#include <cmath>

namespace flux::fx {

namespace {

constexpr float kPi = 3.14159265f;

Color mix(const Color &a, const Color &b, float t)
{
    t = std::clamp(t, 0.f, 1.f);
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}

Color scale(const Color &c, float k)
{
    return {c.r * k, c.g * k, c.b * k};
}

Color add(const Color &a, const Color &b)
{
    return {a.r + b.r, a.g + b.g, a.b + b.b};
}

Color hsv(float h, float s, float v)
{
    h = h - std::floor(h);
    const float i = std::floor(h * 6.f);
    const float f = h * 6.f - i;
    const float p = v * (1 - s), q = v * (1 - f * s), t = v * (1 - (1 - f) * s);
    switch (int(i) % 6) {
    case 0: return {v, t, p};
    case 1: return {q, v, p};
    case 2: return {p, v, t};
    case 3: return {p, q, v};
    case 4: return {t, p, v};
    default: return {v, p, q};
    }
}

float smoothstep(float a, float b, float x)
{
    const float t = std::clamp((x - a) / (b - a), 0.f, 1.f);
    return t * t * (3 - 2 * t);
}

} // namespace

const std::vector<EffectInfo> &effects()
{
    static const std::vector<EffectInfo> list = {
        {"soundwave", "Sound Wave", "The waveform of whatever's playing, drawn across the keys.", false, {0x64d2ff, 0xbf5af2, 0xff375f}, true},
        {"spectrum", "Spectrum", "14 frequency bars, one per column, with falling peaks.", false, {0x30d158, 0xffd60a, 0xff375f}, true},
        {"bassdrop", "Bass Drop", "Every kick drum sends a shockwave out from the spacebar.", false, {0xff375f, 0xbf5af2, 0x0a84ff}, true},
        {"aura", "Audio Aura", "A liquid colour field that moves and glows with the music.", false, {0x5e5ce6, 0xff375f, 0x64d2ff}, true},
        {"aurora", "Aurora", "Slow curtains of light drifting across the board.", false, {0x30d158, 0x64d2ff, 0xbf5af2}},
        {"plasma", "Plasma", "Liquid colour fields that fold into each other.", false, {0xff375f, 0x5e5ce6, 0x64d2ff}},
        {"ocean", "Ocean", "Rolling waves with foam on the crests.", false, {0x0a3d91, 0x0a84ff, 0x63e6e2}},
        {"ember", "Ember", "Flickering flames rising from the space bar.", false, {0xff2d00, 0xff9f0a, 0xffd60a}},
        {"rain", "Neon Rain", "Falling streaks with glowing heads.", false, {0x30d158, 0x63e6e2, 0xffffff}},
        {"starlight", "Starlight", "Keys twinkle on and off like a night sky.", false, {0x5e5ce6, 0x64d2ff, 0xffffff}},
        {"spin", "Spectrum Spin", "A full rainbow rotating around the centre.", false, {0xff453a, 0x30d158, 0x0a84ff}},
        {"drift", "Gradient Drift", "Your palette as a gradient that slowly turns.", false, {0xff375f, 0xff9f0a, 0xbf5af2}},
        {"breathe", "Breathe", "The whole board breathes through your palette.", false, {0x0a84ff, 0xbf5af2, 0x63e6e2}},
        {"ripple", "Ripples", "Each keypress sends a wave out from the key. Press deeper for a bigger wave.", true, {0x64d2ff, 0xbf5af2, 0xff375f}},
        {"flutter", "Flutter", "Sparks burst from the key and flutter around it.", true, {0xffd60a, 0xff9f0a, 0xff375f}},
        {"shockwave", "Shockwave", "A rainbow ring that shifts colour as it travels.", true, {0xff453a, 0x30d158, 0x0a84ff}},
        {"beams", "Beams", "Light shoots out along the key's row and column.", true, {0x0a84ff, 0x64d2ff, 0xffffff}},
        {"splash", "Splash", "Colour bleeds from the key into its neighbours like ink.", true, {0xbf5af2, 0x5e5ce6, 0x64d2ff}},
        {"fireworks", "Fireworks", "A spark rises from the key and bursts.", true, {0xff375f, 0xffd60a, 0x64d2ff}},
        {"magnet", "Magnetic Field", "Your presses become magnetic poles, with field lines flowing between them.", true, {0xff375f, 0x0a84ff, 0xffffff}},
        {"water", "Water", "Real wave physics. Presses drop stones and the ripples interfere.", true, {0x0a3d91, 0x64d2ff, 0x1c1c6e}},
        {"lightning", "Lightning", "Bolts arc from each press. Hold two keys for a Tesla arc.", true, {0xbf5af2, 0x64d2ff, 0xffffff}},
        {"ink", "Ink", "Colour swirls through a curling flow like smoke in water.", true, {0xff375f, 0x5e5ce6, 0x30d158}},
        {"constellation", "Constellation", "Each press becomes a star, joined into a constellation.", true, {0x64d2ff, 0xffd60a, 0xbf5af2}},
        {"life", "Life", "Conway's Game of Life, seeded by your typing.", true, {0x30d158, 0x63e6e2, 0xffd60a}},
        {"tempo", "Tempo", "Learns your tapping BPM and pulses on the beat. Made for osu!", true, {0xff375f, 0xbf5af2, 0x0a84ff}},
        {"echo", "Echo", "Each press echoes ping-pong across the board like a stereo delay.", true, {0x64d2ff, 0xff9f0a, 0xbf5af2}},
        {"depth", "Depth Glow", "Keys glow brighter the deeper you press them.", true, {0x0a84ff, 0xbf5af2, 0xffffff}},
        {"heatmap", "Heatmap", "Keys you hit most heat up, then cool down.", true, {0x0a3d91, 0xff375f, 0xffd60a}},
    };
    return list;
}

void Engine::setLayout(const std::vector<KeyPos> &keys, float width, float height)
{
    std::lock_guard lock(m_mutex);
    m_keys = keys;
    m_w = width;
    m_h = height;
    m_stateFor.clear();
}

void Engine::setParams(const Params &p)
{
    std::lock_guard lock(m_mutex);
    m_params = p;
}

Params Engine::params() const
{
    std::lock_guard lock(m_mutex);
    return m_params;
}

bool Engine::active() const
{
    std::lock_guard lock(m_mutex);
    return !m_params.effect.empty();
}

bool Engine::needsTravel() const
{
    std::lock_guard lock(m_mutex);
    for (const auto &e : effects())
        if (m_params.effect == e.id)
            return e.usesTravel;
    return false;
}

float Engine::noise(float x, float y, float t) const
{
    // Cheap smooth pseudo-noise from layered sines, roughly in [-1, 1].
    float n = std::sin(x * 1.7f + t) * std::sin(y * 2.3f - t * 0.7f);
    n += 0.6f * std::sin((x + y) * 0.9f + t * 1.3f);
    n += 0.35f * std::sin(x * 3.1f - y * 1.7f - t * 1.9f);
    return n / 1.95f;
}

Color Engine::pal(float t) const
{
    t = t - std::floor(t);
    const auto &p = m_params.palette;
    const float s = t * 3.f;
    const int i = int(s) % 3;
    const float f = smoothstep(0.f, 1.f, s - std::floor(s));
    return mix(p[i], p[(i + 1) % 3], f);
}

std::vector<Color> Engine::render(double tSec, const std::vector<float> &travel)
{
    std::lock_guard lock(m_mutex);
    const size_t n = m_keys.size();
    std::vector<Color> out(n);
    if (m_params.effect.empty() || n == 0)
        return out;

    if (m_stateFor != m_params.effect) {
        m_stateFor = m_params.effect;
        m_glow.assign(n, 0.f);
        m_heat.assign(n, 0.f);
        m_drops.assign(size_t(std::ceil(m_w)) + 1, 0.f);
        for (auto &d : m_drops)
            d = std::uniform_real_distribution<float>(-6.f, 0.f)(m_rng);
        m_prevTravel.assign(n, 0.f);
        m_ripples.clear();
        m_particles.clear();
        m_beams.clear();
        m_neighbours.assign(n, {});
        for (size_t a = 0; a < n; ++a)
            for (size_t b = 0; b < n; ++b) {
                if (a == b || !m_keys[a].present || !m_keys[b].present)
                    continue;
                const float dx = m_keys[a].x - m_keys[b].x, dy = m_keys[a].y - m_keys[b].y;
                const float d = std::sqrt(dx * dx + dy * dy);
                if (d < 1.7f)
                    m_neighbours[a].push_back({b, 1.f / (0.6f + d)});
            }
        m_lastT = tSec;
    }
    const float dt = float(std::clamp(tSec - m_lastT, 0.0, 0.2));
    m_lastT = tSec;
    const float sp = std::clamp(m_params.speed, 0.1f, 4.f);
    const float t = float(tSec) * sp;
    const std::string &fx = m_params.effect;
    const float cx = m_w / 2, cy = m_h / 2;
    std::uniform_real_distribution<float> uni(0.f, 1.f);

    auto travelAt = [&](size_t i) { return i < travel.size() ? travel[i] : 0.f; };

    // Press detection shared by reactive effects.
    std::vector<bool> pressed(n, false);
    std::vector<float> depth(n, 0.f);
    if (!travel.empty()) {
        const float th = std::max(0.3f, m_params.actuationMm);
        for (size_t i = 0; i < n; ++i) {
            const float tr = travelAt(i);
            pressed[i] = m_keys[i].present && m_prevTravel[i] < th && tr >= th;
            depth[i] = std::clamp(tr / 3.5f, 0.f, 1.f);
            m_prevTravel[i] = tr;
        }
    }
    auto nextColor = [&] { return m_params.palette[(m_colorIdx++) % 3]; };

    if (fx == "rain") {
        for (auto &d : m_drops) {
            d += dt * 4.5f * sp;
            if (d > m_h + 4)
                d = std::uniform_real_distribution<float>(-5.f, -0.5f)(m_rng);
        }
    }
    if (fx == "starlight") {
        for (size_t i = 0; i < n; ++i) {
            m_glow[i] = std::max(0.f, m_glow[i] - dt * 0.9f * sp);
            if (m_keys[i].present && uni(m_rng) < dt * 0.35f * sp)
                m_glow[i] = 1.f;
        }
    }
    if (fx == "ripple" || fx == "shockwave") {
        for (size_t i = 0; i < n; ++i)
            if (pressed[i])
                m_ripples.push_back({m_keys[i].x, m_keys[i].y, float(tSec), 0.6f + 0.8f * depth[i], nextColor()});
        m_ripples.erase(std::remove_if(m_ripples.begin(), m_ripples.end(), [&](const Ripple &r) { return tSec - r.t0 > 1.8 / sp; }),
                        m_ripples.end());
        if (m_ripples.size() > 24)
            m_ripples.erase(m_ripples.begin(), m_ripples.end() - 24);
    }
    if (fx == "flutter" || fx == "fireworks") {
        std::normal_distribution<float> gauss(0.f, 1.f);
        for (size_t i = 0; i < n; ++i) {
            if (!pressed[i])
                continue;
            const Color c = nextColor();
            if (fx == "flutter") {
                const int count = 6 + int(depth[i] * 6);
                for (int p = 0; p < count; ++p) {
                    const float a = uni(m_rng) * 2 * kPi, v = 1.2f + uni(m_rng) * 2.2f;
                    m_particles.push_back({m_keys[i].x, m_keys[i].y, std::cos(a) * v, std::sin(a) * v * 0.6f, 0, 0.9f + uni(m_rng) * 0.8f,
                                           uni(m_rng) * 6.f, mix(c, m_params.palette[(m_colorIdx + p) % 3], uni(m_rng) * 0.5f), false});
                }
            } else {
                m_particles.push_back({m_keys[i].x, m_keys[i].y, gauss(m_rng) * 0.3f, -5.5f, 0, 0.45f, 0, c, true});
            }
        }
        std::vector<Particle> born;
        for (auto &p : m_particles) {
            p.life += dt * sp;
            if (p.rocket && p.life >= p.maxLife) {
                for (int k = 0; k < 14; ++k) {
                    const float a = k / 14.f * 2 * kPi + uni(m_rng) * 0.3f, v = 2.5f + uni(m_rng) * 1.5f;
                    born.push_back({p.x, p.y, std::cos(a) * v, std::sin(a) * v * 0.7f, 0, 0.8f + uni(m_rng) * 0.5f, 0,
                                    mix(p.c, m_params.palette[k % 3], 0.35f), false});
                }
                continue;
            }
            // Flutter: wobble perpendicular to motion and slow down.
            const float wob = p.rocket ? 0.f : std::sin(p.life * 9.f + p.phase) * 2.2f;
            p.x += (p.vx + (-p.vy) * wob * 0.25f) * dt * sp;
            p.y += (p.vy + p.vx * wob * 0.25f + (p.rocket ? 0.f : 0.6f * p.life)) * dt * sp;
            p.vx *= std::pow(0.25f, dt * sp);
            p.vy *= std::pow(0.25f, dt * sp);
        }
        m_particles.erase(std::remove_if(m_particles.begin(), m_particles.end(), [](const Particle &p) { return p.life >= p.maxLife; }),
                          m_particles.end());
        m_particles.insert(m_particles.end(), born.begin(), born.end());
        if (m_particles.size() > 220)
            m_particles.erase(m_particles.begin(), m_particles.end() - 220);
    }
    if (fx == "beams") {
        for (size_t i = 0; i < n; ++i)
            if (pressed[i])
                m_beams.push_back({m_keys[i].x, m_keys[i].y, float(tSec), nextColor()});
        m_beams.erase(std::remove_if(m_beams.begin(), m_beams.end(), [&](const Beam &b) { return tSec - b.t0 > 1.4 / sp; }), m_beams.end());
        if (m_beams.size() > 16)
            m_beams.erase(m_beams.begin(), m_beams.end() - 16);
    }
    if (fx == "splash") {
        for (size_t i = 0; i < n; ++i)
            if (pressed[i])
                m_ripples.push_back({m_keys[i].x, m_keys[i].y, float(tSec), 0.7f + 0.6f * depth[i], nextColor()});
        m_ripples.erase(std::remove_if(m_ripples.begin(), m_ripples.end(), [&](const Ripple &r) { return tSec - r.t0 > 2.5 / sp; }),
                        m_ripples.end());
        if (m_ripples.size() > 24)
            m_ripples.erase(m_ripples.begin(), m_ripples.end() - 24);
    }
    if (fx == "heatmap") {
        for (size_t i = 0; i < n; ++i) {
            m_heat[i] *= std::pow(0.5f, dt / (6.f / sp));
            if (pressed[i])
                m_heat[i] = std::min(1.f, m_heat[i] + 0.22f);
        }
    }
    if (fx == "depth") {
        for (size_t i = 0; i < n; ++i) {
            const float d = std::clamp(travelAt(i) / 3.5f, 0.f, 1.f);
            m_glow[i] = std::max(d, m_glow[i] - dt * 1.6f);
        }
    }

    updateExtra(fx, tSec, dt, sp, pressed, depth, travel);

    for (size_t i = 0; i < n; ++i) {
        const KeyPos &k = m_keys[i];
        if (!k.present)
            continue;
        const float x = k.x, y = k.y;
        Color c;
        if (renderExtra(fx, i, x, y, t, tSec, c)) {
            // handled by a signature effect
        } else if (fx == "aurora") {
            // Two drifting curtain layers, brighter towards the top like real auroras.
            const float c1 = 0.5f + 0.5f * std::sin(x * 0.55f + t * 0.45f + 2.0f * noise(x * 0.15f, y * 0.25f, t * 0.18f));
            const float c2 = 0.5f + 0.5f * std::sin(x * 0.32f - t * 0.31f + 1.4f * noise(x * 0.22f + 3.f, y * 0.3f, t * 0.12f));
            const float curtain = std::max(smoothstep(0.25f, 1.f, c1), 0.8f * smoothstep(0.35f, 1.f, c2));
            const float vert = 0.55f + 0.45f * (1.f - y / m_h) + 0.15f * std::sin(y * 1.3f - t * 0.9f + x * 0.2f);
            c = scale(pal(x / m_w * 0.5f + t * 0.025f + 0.2f * noise(x * 0.5f, y, t * 0.3f)), 0.10f + 0.9f * curtain * std::clamp(vert, 0.f, 1.f));
        } else if (fx == "plasma") {
            float v = std::sin(x * 0.55f + t) + std::sin((y * 0.9f + t * 0.7f)) + std::sin((x * 0.4f + y * 0.6f + t * 1.1f));
            const float dx = x - cx - 3 * std::sin(t * 0.33f), dy = y - cy - 1.5f * std::cos(t * 0.5f);
            v += std::sin(std::sqrt(dx * dx + dy * dy) * 0.9f - t);
            c = pal(v / 8.f + 0.5f + t * 0.02f);
        } else if (fx == "ocean") {
            const float w = std::sin(x * 0.75f - t * 1.6f + std::sin(y * 0.8f + t * 0.4f)) * 0.6f + std::sin(x * 0.35f + y * 0.5f - t * 0.9f) * 0.4f;
            const float h = 0.5f + 0.5f * w;
            c = mix(m_params.palette[0], m_params.palette[1], h);
            c = add(c, scale(m_params.palette[2], smoothstep(0.82f, 1.f, h) * 0.9f));
        } else if (fx == "ember") {
            const float rise = 1.f - y / m_h;   // 0 bottom .. 1 top
            const float flick = 0.5f + 0.5f * noise(x * 0.9f, y * 1.4f + t * 1.8f, t * 2.2f);
            const float heat = std::clamp((1.f - rise * 1.05f) * (0.55f + 0.75f * flick), 0.f, 1.f);
            c = heat < 0.5f ? mix({0, 0, 0}, m_params.palette[0], heat * 2) : mix(m_params.palette[0], mix(m_params.palette[1], m_params.palette[2], (heat - 0.5f) * 2), (heat - 0.5f) * 2);
        } else if (fx == "rain") {
            const size_t col = size_t(std::clamp(int(std::round(x)), 0, int(m_drops.size()) - 1));
            const float head = m_drops[col];
            const float d = head - y;
            if (d >= -0.5f && d < 4.f) {
                const float tail = std::exp(-std::max(0.f, d) * 0.9f);
                c = d < 0.5f ? m_params.palette[2] : scale(mix(m_params.palette[1], m_params.palette[0], std::min(1.f, d / 3.f)), tail);
            }
            c = add(c, scale(m_params.palette[0], 0.04f));
        } else if (fx == "starlight") {
            const float g = m_glow[i];
            const float tw = g * g * (3 - 2 * g);
            c = add(scale(m_params.palette[0], 0.07f), scale(mix(m_params.palette[1], m_params.palette[2], g), tw));
        } else if (fx == "spin") {
            const float a = std::atan2((y - cy) * 2.f, x - cx) / (2 * kPi);
            c = hsv(a + t * 0.12f, 1.f, 1.f);
        } else if (fx == "drift") {
            const float ang = t * 0.15f;
            const float u = ((x - cx) * std::cos(ang) + (y - cy) * 2.f * std::sin(ang)) / m_w;
            c = pal(u * 0.9f + 0.5f + t * 0.02f);
        } else if (fx == "breathe") {
            // Apple's "sleep light" style curve: smooth in, longer glow, smooth out.
            const float ph = std::fmod(t * 0.25f, 1.f);
            const float b = std::pow(0.5f - 0.5f * std::cos(ph * 2 * kPi), 1.6f);
            c = scale(pal(std::floor(t * 0.25f) / 3.f + x / m_w * 0.15f), 0.06f + 0.94f * b);
        } else if (fx == "depth") {
            const float g = m_glow[i];
            const Color base = scale(m_params.palette[0], 0.06f);
            c = g < 0.5f ? mix(base, m_params.palette[1], g * 2) : mix(m_params.palette[1], m_params.palette[2], (g - 0.5f) * 2);
        } else if (fx == "ripple") {
            c = scale(m_params.palette[0], 0.05f);
            for (const auto &r : m_ripples) {
                const float age = float(tSec) - r.t0;
                const float radius = age * 7.f * sp;
                const float dx = x - r.x, dy = (y - r.y) * 1.1f;
                const float dist = std::sqrt(dx * dx + dy * dy);
                const float ring = std::exp(-std::pow((dist - radius) / (0.6f + 0.5f * r.strength), 2.f));
                const float fade = std::max(0.f, 1.f - age * sp / (1.0f + 0.8f * r.strength));
                c = add(c, scale(r.c, ring * fade * r.strength));
                if (dist < 0.6f)
                    c = add(c, scale({1, 1, 1}, std::max(0.f, 0.6f - age * 3.f)));
            }
        } else if (fx == "shockwave") {
            c = scale(hsv(t * 0.05f, 0.8f, 1.f), 0.04f);
            for (const auto &r : m_ripples) {
                const float age = float(tSec) - r.t0;
                const float radius = age * 8.f * sp;
                const float dx = x - r.x, dy = (y - r.y) * 1.1f;
                const float dist = std::sqrt(dx * dx + dy * dy);
                const float ring = std::exp(-std::pow((dist - radius) / (0.7f + 0.4f * r.strength), 2.f));
                const float fade = std::max(0.f, 1.f - age * sp / 1.8f);
                c = add(c, scale(hsv(dist * 0.07f - age * 0.4f + r.x * 0.02f, 1.f, 1.f), ring * fade * r.strength));
            }
        } else if (fx == "flutter" || fx == "fireworks") {
            c = scale(m_params.palette[1], 0.03f);
            for (const auto &p : m_particles) {
                const float dx = x - p.x, dy = (y - p.y) * 1.15f;
                const float d2 = dx * dx + dy * dy;
                if (d2 > 2.2f)
                    continue;
                const float life = 1.f - p.life / p.maxLife;
                const float twinkle = p.rocket ? 1.f : 0.65f + 0.35f * std::sin(p.life * 22.f + p.phase);
                c = add(c, scale(p.c, std::exp(-d2 / 0.35f) * life * twinkle * 1.3f));
            }
        } else if (fx == "beams") {
            c = scale(m_params.palette[0], 0.04f);
            for (const auto &b : m_beams) {
                const float age = float(tSec) - b.t0;
                const float head = age * 14.f * sp;
                const float fade = std::max(0.f, 1.f - age * sp / 1.4f);
                const bool sameRow = std::abs(y - b.y) < 0.5f;
                const bool sameCol = std::abs(x - b.x) < 0.75f;
                float k = 0;
                if (sameRow) {
                    const float d = std::abs(x - b.x);
                    k = std::max(k, d <= head ? std::exp(-(head - d) * 0.35f) : 0.f);
                }
                if (sameCol) {
                    const float d = std::abs(y - b.y) * 2.5f;
                    k = std::max(k, d <= head ? std::exp(-(head - d) * 0.35f) : 0.f);
                }
                if (sameRow && sameCol)
                    k = std::max(k, std::max(0.f, 1.f - age * 2.5f));
                c = add(c, scale(mix(b.c, m_params.palette[2], k * 0.5f), std::min(1.f, k * 1.4f) * fade));
            }
        } else if (fx == "splash") {
            // Ink-drop bleed: a soft blob that widens while it fades.
            c = scale(m_params.palette[1], 0.03f);
            for (const auto &r : m_ripples) {
                const float age = std::max(0.f, float(tSec) - r.t0) * sp;
                const float dx = x - r.x, dy = (y - r.y) * 1.2f;
                const float spread = 0.25f + age * 2.6f;
                const float blob = std::exp(-(dx * dx + dy * dy) / spread) * std::exp(-age * 1.1f) * r.strength * 1.6f;
                c = add(c, scale(mix(r.c, {1, 1, 1}, std::max(0.f, 0.35f - age)), blob));
            }
        } else if (fx == "heatmap") {
            const float h = m_heat[i];
            c = h < 0.5f ? mix(scale(m_params.palette[0], 0.25f), m_params.palette[1], h * 2) : mix(m_params.palette[1], m_params.palette[2], (h - 0.5f) * 2);
        }
        const float k2 = std::clamp(m_params.intensity, 0.f, 1.f);
        // Hue-preserving clamp: overlapping effects stay colourful instead of
        // clipping to white.
        const float peak = std::max({c.r, c.g, c.b});
        if (peak > 1.f)
            c = scale(c, 1.f / peak);
        // Mild gamma so fades look even on LEDs.
        out[i] = {std::pow(std::clamp(c.r, 0.f, 1.f), 1.6f) * k2, std::pow(std::clamp(c.g, 0.f, 1.f), 1.6f) * k2,
                  std::pow(std::clamp(c.b, 0.f, 1.f), 1.6f) * k2};
    }
    return out;
}

} // namespace flux::fx
