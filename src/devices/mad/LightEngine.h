#pragma once

// Host-side lighting effects for per-key RGB keyboards. Renders a frame of
// 70 colours from key positions, time and (optionally) live key travel.
// Pure C++, runs on the keyboard's I/O thread.

#include <array>
#include <cstdint>
#include <mutex>
#include <random>
#include <string>
#include <vector>

namespace flux::fx {

struct Color {
    float r = 0, g = 0, b = 0;
};

struct KeyPos {
    bool present = false;
    float x = 0, y = 0;   // key centre in key units
};

struct Params {
    std::string effect;             // "" = off
    float speed = 1.0f;             // 0.2 .. 3
    float intensity = 1.0f;         // 0 .. 1, overall brightness scaling
    std::array<Color, 3> palette{Color{0.04f, 0.52f, 1.0f}, Color{0.75f, 0.35f, 0.95f}, Color{0.39f, 0.82f, 1.0f}};
    float actuationMm = 1.2f;       // press threshold for reactive effects
};

struct EffectInfo {
    const char *id;
    const char *name;
    const char *blurb;
    bool usesTravel;
    std::array<uint32_t, 3> palette; // default palette 0xRRGGBB
    bool usesAudio = false;
};

struct AudioFrame {
    std::array<float, 14> bands{};
    std::array<float, 28> wave{};
    float level = 0, bass = 0, treble = 0;
    uint32_t beats = 0;
    bool active = false;
};

const std::vector<EffectInfo> &effects();

class Engine {
public:
    void setLayout(const std::vector<KeyPos> &keys, float width, float height);
    void setParams(const Params &p);
    Params params() const;
    bool active() const;
    bool needsTravel() const;
    bool needsAudio() const;
    void setAudio(const AudioFrame &a);

    // travelMm: per-key travel (may be empty when not needed)
    std::vector<Color> render(double tSeconds, const std::vector<float> &travelMm);

private:
    void updateExtra(const std::string &fx, double tSec, float dt, float sp, const std::vector<bool> &pressed,
                     const std::vector<float> &depth, const std::vector<float> &travel);
    bool renderExtra(const std::string &fx, size_t i, float x, float y, float t, double tSec, Color &out);
    float gridSample(const std::vector<float> &g, float x, float y) const;
    float noise(float x, float y, float t) const;
    Color pal(float t) const;

    mutable std::mutex m_mutex;
    Params m_params;
    std::vector<KeyPos> m_keys;
    float m_w = 15, m_h = 5;

    // Effect state
    std::mt19937 m_rng{1234};
    double m_lastT = 0;
    std::vector<float> m_glow;          // depth-glow afterglow / starfield
    std::vector<float> m_heat;          // fire / heatmap
    std::vector<float> m_drops;         // matrix: per-column head position
    std::vector<float> m_prevTravel;
    struct Ripple { float x, y, t0, strength; Color c; };
    std::vector<Ripple> m_ripples;
    struct Particle { float x, y, vx, vy, life, maxLife, phase; Color c; bool rocket; };
    std::vector<Particle> m_particles;
    struct Beam { float x, y, t0; Color c; };
    std::vector<Beam> m_beams;
    std::vector<std::vector<std::pair<size_t, float>>> m_neighbours; // splash diffusion
    int m_colorIdx = 0;
    std::string m_stateFor;

    // Extra effects
    std::string m_extraFor;
    AudioFrame m_audio;
    uint32_t m_lastBeats = 0;
    std::array<float, 14> m_peaks{};
    int m_gw = 0, m_gh = 0;                    // simulation grid (2 cells per key unit)
    std::vector<float> m_simA, m_simB;         // water heights / ink dye
    std::vector<float> m_dyeR, m_dyeG, m_dyeB;
    struct Pole { float x, y, q, t0; };
    std::vector<Pole> m_poles;
    struct Star { float x, y, t0; Color c; };
    std::vector<Star> m_stars;
    struct Bolt { std::vector<std::array<float, 2>> pts; float t0; Color c; };
    std::vector<Bolt> m_bolts;
    double m_lastArc = 0;
    std::vector<uint8_t> m_alive;
    std::vector<float> m_age;
    double m_lifeT = 0, m_lifeQuiet = 0;
    std::vector<double> m_taps;
    double m_beatPeriod = 0, m_beatT0 = 0;
    long m_lastBeatIdx = -1;
};

} // namespace flux::fx
