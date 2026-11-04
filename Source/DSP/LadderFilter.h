#pragma once

#include <cmath>
#include <algorithm>

namespace cuarzo
{
inline float fastTanh (float x) noexcept
{
    if (x >  3.0f) return  1.0f;
    if (x < -3.0f) return -1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// Filtro "ladder" de 4 polos modelado (topologia TPT / zero-delay feedback) con
// saturacion tanh dentro del lazo de realimentacion y mezcla de modos estilo Xpander.
class LadderFilter
{
public:
    enum Mode { lp24 = 0, lp12, bp12, hp12, hp24, notch };

    void prepare (double sr) noexcept { sampleRate = sr; reset(); }
    void reset() noexcept { s1 = s2 = s3 = s4 = 0.0f; }
    void setMode (int m) noexcept { mode = m; }

    // cutoffHz en Hz, reso 0..1 (auto-oscila cerca de 1), drive 0..1
    void setParameters (float cutoffHz, float reso, float drive) noexcept
    {
        const float fc = std::clamp (cutoffHz, 10.0f, (float) (sampleRate * 0.45));
        g     = std::tan (3.14159265f * fc / (float) sampleRate);
        G     = g / (1.0f + g);
        beta  = 1.0f / (1.0f + g);
        k     = 4.0f * std::clamp (reso, 0.0f, 1.0f) * 0.98f;
        driveGain = 1.0f + drive * drive * 24.0f;
        outGain   = 1.0f / std::pow (driveGain, 0.55f);
    }

    inline float process (float x) noexcept
    {
        const float G2 = G * G;
        const float G3 = G2 * G;
        const float G4 = G2 * G2;

        // Compensacion parcial de la perdida de ganancia por resonancia (modos paso bajo)
        const float in = (mode <= lp12) ? x * (1.0f + 0.5f * k) : x;

        const float S = beta * (G3 * s1 + G2 * s2 + G * s3 + s4);
        float u = (in - k * S) / (1.0f + k * G4);
        u = fastTanh (u * driveGain * 0.5f) * 2.0f;   // saturacion analoga en la entrada del lazo

        float v;
        v = (u  - s1) * G; const float y1 = v + s1; s1 = y1 + v;
        v = (y1 - s2) * G; const float y2 = v + s2; s2 = y2 + v;
        v = (y2 - s3) * G; const float y3 = v + s3; s3 = y3 + v;
        v = (y3 - s4) * G; const float y4 = v + s4; s4 = y4 + v;

        float out;
        switch (mode)
        {
            case lp12:  out = y2; break;
            case bp12:  out = 2.0f * (y1 - y2); break;
            case hp12:  out = u - 2.0f * y1 + y2; break;
            case hp24:  out = u - 4.0f * y1 + 6.0f * y2 - 4.0f * y3 + y4; break;
            case notch: out = u - 2.0f * y1 + 2.0f * y2; break;
            case lp24:
            default:    out = y4; break;
        }
        return out * outGain;
    }

private:
    double sampleRate = 44100.0;
    int mode = lp24;
    float g = 0.5f, G = 0.33f, beta = 0.66f, k = 0.0f, driveGain = 1.0f, outGain = 1.0f;
    float s1 = 0.0f, s2 = 0.0f, s3 = 0.0f, s4 = 0.0f;
};
} // namespace cuarzo
