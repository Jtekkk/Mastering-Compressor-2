#pragma once

#include <cmath>

namespace mc2
{

// RBJ cookbook biquad, double-precision coefficients/state (needed for the
// very low corner frequencies used by the transformer model at high rates).
struct Biquad
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double z1 = 0.0, z2 = 0.0;

    void reset() noexcept { z1 = z2 = 0.0; }

    void identity() noexcept
    {
        b0 = 1.0; b1 = b2 = a1 = a2 = 0.0;
    }

    inline float process (float xin) noexcept
    {
        const double x = xin;
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return static_cast<float> (y);
    }

    void highpass (double fs, double f, double Q) noexcept
    {
        const double w0 = 2.0 * pi() * f / fs;
        const double cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * Q);
        const double a0 = 1.0 + alpha;
        b0 = (1.0 + cw) * 0.5 / a0;
        b1 = -(1.0 + cw) / a0;
        b2 = (1.0 + cw) * 0.5 / a0;
        a1 = -2.0 * cw / a0;
        a2 = (1.0 - alpha) / a0;
    }

    void peak (double fs, double f, double gainDB, double Q) noexcept
    {
        const double A = std::pow (10.0, gainDB / 40.0);
        const double w0 = 2.0 * pi() * f / fs;
        const double cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * Q);
        const double a0 = 1.0 + alpha / A;
        b0 = (1.0 + alpha * A) / a0;
        b1 = -2.0 * cw / a0;
        b2 = (1.0 - alpha * A) / a0;
        a1 = -2.0 * cw / a0;
        a2 = (1.0 - alpha / A) / a0;
    }

    void lowShelf (double fs, double f, double gainDB, double Q) noexcept
    {
        const double A = std::pow (10.0, gainDB / 40.0);
        const double w0 = 2.0 * pi() * f / fs;
        const double cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * Q);
        const double sq = 2.0 * std::sqrt (A) * alpha;
        const double a0 = (A + 1.0) + (A - 1.0) * cw + sq;
        b0 = A * ((A + 1.0) - (A - 1.0) * cw + sq) / a0;
        b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cw) / a0;
        b2 = A * ((A + 1.0) - (A - 1.0) * cw - sq) / a0;
        a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cw) / a0;
        a2 = ((A + 1.0) + (A - 1.0) * cw - sq) / a0;
    }

    void highShelf (double fs, double f, double gainDB, double Q) noexcept
    {
        const double A = std::pow (10.0, gainDB / 40.0);
        const double w0 = 2.0 * pi() * f / fs;
        const double cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * Q);
        const double sq = 2.0 * std::sqrt (A) * alpha;
        const double a0 = (A + 1.0) - (A - 1.0) * cw + sq;
        b0 = A * ((A + 1.0) + (A - 1.0) * cw + sq) / a0;
        b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cw) / a0;
        b2 = A * ((A + 1.0) + (A - 1.0) * cw - sq) / a0;
        a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cw) / a0;
        a2 = ((A + 1.0) - (A - 1.0) * cw - sq) / a0;
    }

private:
    static constexpr double pi() noexcept { return 3.14159265358979323846; }
};

} // namespace mc2
