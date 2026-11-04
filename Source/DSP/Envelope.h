#pragma once

#include <cmath>
#include <algorithm>

namespace cuarzo
{
// ADSR con curvas de estilo analogico: ataque convexo y decaimiento/liberacion exponenciales.
class Envelope
{
public:
    enum class Stage { idle, attack, decay, sustain, release };

    void setSampleRate (double newRate) noexcept
    {
        sampleRate = newRate;
        lastA = lastD = lastR = -1.0f; // fuerza recalculo
    }

    void setParameters (float attackS, float decayS, float sustainLevel, float releaseS) noexcept
    {
        sustain = std::clamp (sustainLevel, 0.0f, 1.0f);

        if (std::abs (attackS - lastA) > 1.0e-7f)
        {
            lastA = attackS;
            // nivel(t) = kOvershoot * (1 - e^(-t/tau)) alcanza 1.0 en t = attack
            const double tau = std::max (1.0e-4, (double) attackS) / std::log (kOvershoot / (kOvershoot - 1.0));
            attackCoef = (float) (1.0 - std::exp (-1.0 / (tau * sampleRate)));
        }
        if (std::abs (decayS - lastD) > 1.0e-7f)
        {
            lastD = decayS;
            decayCoef = coefFor (decayS);
        }
        if (std::abs (releaseS - lastR) > 1.0e-7f)
        {
            lastR = releaseS;
            releaseCoef = coefFor (releaseS);
        }
    }

    void noteOn() noexcept   { stage = Stage::attack; }   // conserva el nivel actual => sin clics al redisparar
    void noteOff() noexcept  { if (stage != Stage::idle) stage = Stage::release; }
    void reset() noexcept    { stage = Stage::idle; level = 0.0f; }

    bool isActive() const noexcept { return stage != Stage::idle; }
    Stage getStage() const noexcept { return stage; }
    float getLevel() const noexcept { return level; }

    inline float next() noexcept
    {
        switch (stage)
        {
            case Stage::attack:
                level += ((float) kOvershoot - level) * attackCoef;
                if (level >= 1.0f) { level = 1.0f; stage = Stage::decay; }
                break;

            case Stage::decay:
                level = sustain + (level - sustain) * decayCoef;
                if (std::abs (level - sustain) < 1.0e-4f) { level = sustain; stage = Stage::sustain; }
                break;

            case Stage::sustain:
                level += (sustain - level) * 0.002f; // sigue cambios del sostenido suavemente
                break;

            case Stage::release:
                level *= releaseCoef;
                if (level < 1.0e-4f) { level = 0.0f; stage = Stage::idle; }
                break;

            case Stage::idle:
            default:
                level = 0.0f;
                break;
        }
        return level;
    }

private:
    static constexpr double kOvershoot = 1.3;

    float coefFor (float seconds) const noexcept
    {
        // cae a -60 dB en el tiempo indicado
        const double t = std::max (1.0e-4, (double) seconds);
        return (float) std::exp (std::log (0.001) / (t * sampleRate));
    }

    double sampleRate = 44100.0;
    Stage stage = Stage::idle;
    float level = 0.0f, sustain = 1.0f;
    float attackCoef = 0.01f, decayCoef = 0.999f, releaseCoef = 0.999f;
    float lastA = -1.0f, lastD = -1.0f, lastR = -1.0f;
};
} // namespace cuarzo
