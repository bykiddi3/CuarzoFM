#pragma once

#include "LadderFilter.h"
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace cuarzo
{
// Generador de ruido rapido (xorshift32), salida -1..1
struct Noise
{
    uint32_t state = 0x9E3779B9u;
    void seed (uint32_t s) noexcept { state = s ? s : 0x9E3779B9u; }
    inline float next() noexcept
    {
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        return (float) (int32_t) state * (1.0f / 2147483648.0f);
    }
};

// LFO bipolar (-1..1), utilizable a velocidad de audio
class LFO
{
public:
    enum Shape { sine = 0, triangle, saw, square, sampleHold };

    void reset (float startPhase = 0.0f) noexcept { phase = startPhase; held = noise.next(); }
    void seed (uint32_t s) noexcept { noise.seed (s); }

    inline float next (int shape, float inc) noexcept
    {
        float v;
        switch (shape)
        {
            case triangle:   v = 1.0f - 4.0f * std::abs (phase - 0.5f); break;
            case saw:        v = 2.0f * phase - 1.0f; break;
            case square:     v = phase < 0.5f ? 1.0f : -1.0f; break;
            case sampleHold: v = held; break;
            case sine:
            default:         v = std::sin (6.28318530718f * phase); break;
        }
        phase += inc;
        if (phase >= 1.0f)
        {
            phase -= std::floor (phase);
            held = noise.next();
        }
        return v;
    }

private:
    float phase = 0.0f, held = 0.0f;
    Noise noise;
};

// Etapa de distorsion global (post-filtro)
class Distortion
{
public:
    enum Type { off = 0, overdrive, saturation, fuzz, wavefolder, bitcrusher };

    void prepare (double sr) noexcept { sampleRate = sr; reset(); }
    void reset() noexcept { dcX = dcY = 0.0f; holdCount = 0; held = 0.0f; }

    inline float process (float x, int type, float amount) noexcept
    {
        float y;
        switch (type)
        {
            case overdrive:
            {
                const float g = 1.0f + amount * 20.0f;
                const float b = 0.25f * amount;                     // asimetria => armonicos pares
                y = (fastTanh (g * x + b) - fastTanh (b)) / std::max (0.3f, fastTanh (g * 0.5f));
                y *= 0.7f;
                break;
            }
            case saturation:
            {
                const float g = 1.0f + amount * 5.0f;
                float s = std::clamp (g * x, -1.5f, 1.5f);
                y = (s - (4.0f / 27.0f) * s * s * s) / (1.0f + amount);   // curva cubica suave (tipo cinta)
                break;
            }
            case fuzz:
            {
                const float g = 1.0f + amount * 60.0f;
                y = std::clamp (g * x, -1.0f, 1.0f) * 0.6f;
                break;
            }
            case wavefolder:
            {
                const float g = 1.0f + amount * 8.0f;
                y = std::sin (g * x * 1.5707963f) * 0.8f;
                break;
            }
            case bitcrusher:
            {
                const int factor = 1 + (int) (amount * amount * 31.0f);      // reduccion de frecuencia
                if (holdCount <= 0) { held = x; holdCount = factor; }
                --holdCount;
                const float levels = std::pow (2.0f, 16.0f - amount * 13.0f); // 16 -> 3 bits
                y = std::round (held * levels) / levels;
                break;
            }
            case off:
            default:
                return x;
        }

        // Bloqueador de DC (las curvas asimetricas generan offset)
        const float out = y - dcX + 0.9975f * dcY;
        dcX = y; dcY = out;
        return out;
    }

private:
    double sampleRate = 44100.0;
    float dcX = 0.0f, dcY = 0.0f, held = 0.0f;
    int holdCount = 0;
};
} // namespace cuarzo
