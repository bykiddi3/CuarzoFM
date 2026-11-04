#pragma once

#include <array>

namespace cuarzo
{
// Osciladores: 0 = A, 1 = B, 2 = C, 3 = D.
// La modulacion siempre va de un indice mayor a uno menor, asi que el orden de
// calculo D -> C -> B -> A es valido para todos los algoritmos.
struct Algorithm
{
    const char* name;
    std::array<unsigned char, 4> modulatedBy; // mascara de bits: fuentes que modulan a cada oscilador
    unsigned char carriers;                   // mascara de bits: osciladores que salen al filtro

    bool modulates (int src, int dst) const noexcept { return (modulatedBy[(size_t) dst] >> src) & 1; }
    bool isCarrier (int i) const noexcept            { return (carriers >> i) & 1; }
    int  numCarriers() const noexcept                { int n = 0; for (int i = 0; i < 4; ++i) n += isCarrier (i) ? 1 : 0; return n; }
};

constexpr unsigned char bit (int i) { return (unsigned char) (1 << i); }
enum { A = 0, B = 1, C = 2, D = 3 };

inline const std::array<Algorithm, 11>& algorithms()
{
    static const std::array<Algorithm, 11> algs {{
        { "D > C > B > A",          { bit (B), bit (C), bit (D), 0 },                               bit (A) },
        { "(C + D) > B > A",        { bit (B), (unsigned char) (bit (C) | bit (D)), 0, 0 },         bit (A) },
        { "(B + D>C) > A",          { (unsigned char) (bit (B) | bit (C)), 0, bit (D), 0 },         bit (A) },
        { "D > (B, C) > A",         { (unsigned char) (bit (B) | bit (C)), bit (D), bit (D), 0 },   bit (A) },
        { "(B + C + D) > A",        { (unsigned char) (bit (B) | bit (C) | bit (D)), 0, 0, 0 },     bit (A) },
        { "B > A  |  D > C",        { bit (B), 0, bit (D), 0 },                                     (unsigned char) (bit (A) | bit (C)) },
        { "D > C > B  |  A",        { 0, bit (C), bit (D), 0 },                                     (unsigned char) (bit (A) | bit (B)) },
        { "D > (A, B, C)",          { bit (D), bit (D), bit (D), 0 },                               (unsigned char) (bit (A) | bit (B) | bit (C)) },
        { "D > C  |  A  |  B",      { 0, 0, bit (D), 0 },                                           (unsigned char) (bit (A) | bit (B) | bit (C)) },
        { "(C + D) > A  |  B",      { (unsigned char) (bit (C) | bit (D)), 0, 0, 0 },               (unsigned char) (bit (A) | bit (B)) },
        { "A | B | C | D (paralelo)", { 0, 0, 0, 0 },                                               (unsigned char) (bit (A) | bit (B) | bit (C) | bit (D)) },
    }};
    return algs;
}

// Posiciones para dibujar cada algoritmo (x 0..3, y 0..3; y = 3 es la fila de portadores)
struct OpPos { float x, y; };
inline const std::array<std::array<OpPos, 4>, 11>& algorithmLayouts()
{
    static const std::array<std::array<OpPos, 4>, 11> l {{
        {{ { 1.5f, 3 }, { 1.5f, 2 }, { 1.5f, 1 }, { 1.5f, 0 } }},
        {{ { 1.5f, 3 }, { 1.5f, 2 }, { 1.0f, 1 }, { 2.0f, 1 } }},
        {{ { 1.5f, 3 }, { 1.0f, 2 }, { 2.0f, 2 }, { 2.0f, 1 } }},
        {{ { 1.5f, 3 }, { 1.0f, 2 }, { 2.0f, 2 }, { 1.5f, 1 } }},
        {{ { 1.5f, 3 }, { 0.5f, 2 }, { 1.5f, 2 }, { 2.5f, 2 } }},
        {{ { 1.0f, 3 }, { 1.0f, 2 }, { 2.0f, 3 }, { 2.0f, 2 } }},
        {{ { 1.0f, 3 }, { 2.0f, 3 }, { 2.0f, 2 }, { 2.0f, 1 } }},
        {{ { 0.5f, 3 }, { 1.5f, 3 }, { 2.5f, 3 }, { 1.5f, 2 } }},
        {{ { 0.5f, 3 }, { 1.5f, 3 }, { 2.5f, 3 }, { 2.5f, 2 } }},
        {{ { 1.0f, 3 }, { 2.5f, 3 }, { 0.5f, 2 }, { 1.5f, 2 } }},
        {{ { 0.0f, 3 }, { 1.0f, 3 }, { 2.0f, 3 }, { 3.0f, 3 } }},
    }};
    return l;
}
} // namespace cuarzo
