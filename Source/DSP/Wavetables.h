#pragma once

#include <array>
#include <vector>
#include <cmath>
#include <algorithm>

namespace cuarzo
{
// Tablas de onda con "mipmaps" limitados en banda: cada nivel contiene la mitad
// de armonicos que el anterior, para evitar aliasing en notas agudas.
constexpr int kTableSize  = 2048;
constexpr int kTableMask  = kTableSize - 1;
constexpr int kNumLevels  = 11;   // nivel L contiene hasta (1024 >> L) armonicos

struct MipTable
{
    std::array<std::array<float, kTableSize + 1>, kNumLevels> levels {};

    static const std::array<float, kTableSize>& sineBase()
    {
        static const auto table = []
        {
            std::array<float, kTableSize> t {};
            for (int i = 0; i < kTableSize; ++i)
                t[(size_t) i] = (float) std::sin (2.0 * 3.14159265358979323846 * i / kTableSize);
            return t;
        }();
        return table;
    }

    // amps[n-1] = amplitud del armonico n (fase seno). numHarmonics puede ser > 1023.
    void buildFromHarmonics (const float* amps, int numHarmonics)
    {
        const auto& s = sineBase();
        std::vector<double> acc ((size_t) kTableSize, 0.0);
        const int count = std::min (numHarmonics, 1023);

        int n = 1;
        for (int L = kNumLevels - 1; L >= 0; --L)
        {
            const int limit = std::min (1023, 1024 >> L);
            for (; n <= limit && n <= count; ++n)
            {
                const double a = amps[n - 1];
                if (std::abs (a) < 1.0e-12) continue;
                for (int i = 0; i < kTableSize; ++i)
                    acc[(size_t) i] += a * s[(size_t) ((n * i) & kTableMask)];
            }
            auto& lv = levels[(size_t) L];
            for (int i = 0; i < kTableSize; ++i)
                lv[(size_t) i] = (float) acc[(size_t) i];
        }

        // Normalizacion unica (misma escala en todos los niveles => sin saltos de volumen)
        float peak = 0.0f;
        for (auto& lv : levels)
            for (int i = 0; i < kTableSize; ++i)
                peak = std::max (peak, std::abs (lv[(size_t) i]));

        const float g = peak > 1.0e-6f ? 1.0f / peak : 0.0f;
        for (auto& lv : levels)
        {
            for (int i = 0; i < kTableSize; ++i)
                lv[(size_t) i] *= g;
            lv[kTableSize] = lv[0]; // muestra de guarda para interpolar
        }
    }

    // phaseInc = frecuencia / samplerate
    inline const float* levelFor (float phaseInc) const noexcept
    {
        const float maxH = 0.5f / std::max (std::abs (phaseInc), 1.0e-7f);
        int L = 0;
        while (L < kNumLevels - 1 && (float) (1024 >> L) > maxH)
            ++L;
        return levels[(size_t) L].data();
    }

    static inline float read (const float* t, float phase) noexcept
    {
        const float p = phase * (float) kTableSize;
        const int   i = (int) p;
        const float f = p - (float) i;
        const int   j = i & kTableMask;
        return t[j] + f * (t[j + 1] - t[j]);
    }
};

// Formas clasicas compartidas por todas las instancias
struct ClassicWaves
{
    MipTable sine, triangle, square, saw;

    static const ClassicWaves& get()
    {
        static const ClassicWaves w;
        return w;
    }

private:
    ClassicWaves()
    {
        std::vector<float> a (1023, 0.0f);

        a[0] = 1.0f;
        sine.buildFromHarmonics (a.data(), 1);

        std::fill (a.begin(), a.end(), 0.0f);
        for (int n = 1; n <= 1023; n += 2)
            a[(size_t) n - 1] = (((n - 1) / 2) % 2 == 0 ? 1.0f : -1.0f) / (float) (n * n);
        triangle.buildFromHarmonics (a.data(), 1023);

        std::fill (a.begin(), a.end(), 0.0f);
        for (int n = 1; n <= 1023; n += 2)
            a[(size_t) n - 1] = 1.0f / (float) n;
        square.buildFromHarmonics (a.data(), 1023);

        for (int n = 1; n <= 1023; ++n)
            a[(size_t) n - 1] = 1.0f / (float) n;
        saw.buildFromHarmonics (a.data(), 1023);
    }
};
} // namespace cuarzo
