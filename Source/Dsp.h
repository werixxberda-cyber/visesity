#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

namespace vv
{
constexpr double kPi = 3.14159265358979323846;

// ---------------------------------------------------------------- Biquad (RBJ cookbook)
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    float process (float xf) noexcept
    {
        const double x = xf;
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return (float) y;
    }

    void reset() noexcept { z1 = z2 = 0.0; }

    void setCoeffs (double nb0, double nb1, double nb2, double a0, double na1, double na2) noexcept
    {
        b0 = nb0 / a0; b1 = nb1 / a0; b2 = nb2 / a0; a1 = na1 / a0; a2 = na2 / a0;
    }

    void setHighPass (double sr, double f, double q)
    {
        const double w = 2.0 * kPi * f / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        setCoeffs ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }

    void setBandPass (double sr, double f, double q)
    {
        const double w = 2.0 * kPi * f / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        setCoeffs (al, 0, -al, 1 + al, -2 * c, 1 - al);
    }

    void setPeak (double sr, double f, double q, double dB)
    {
        const double A = std::pow (10.0, dB / 40.0);
        const double w = 2.0 * kPi * f / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        setCoeffs (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }

    void setHighShelf (double sr, double f, double q, double dB)
    {
        const double A = std::pow (10.0, dB / 40.0);
        const double w = 2.0 * kPi * f / sr, c = std::cos (w), s = std::sin (w);
        const double t = 2.0 * std::sqrt (A) * (s / (2.0 * q));
        setCoeffs (A * ((A + 1) + (A - 1) * c + t),
                   -2 * A * ((A - 1) + (A + 1) * c),
                   A * ((A + 1) + (A - 1) * c - t),
                   (A + 1) - (A - 1) * c + t,
                   2 * ((A - 1) - (A + 1) * c),
                   (A + 1) - (A - 1) * c - t);
    }
};

// ---------------------------------------------------------------- Pitch corrector
// Simple "hard-tune" style corrector: YIN pitch detection (on a decimated copy),
// snap to the nearest semitone, delay-line (two crossfaded taps) pitch shifter.
class PitchCorrector
{
public:
    void prepare (double sr)
    {
        dec = std::max (1, (int) std::lround (sr / 16000.0));
        effSr = sr / dec;
        winLen = (int) (0.032 * sr);
        for (auto& b : buf) b.assign (kBuf, 0.0f);
        ana.assign (kAna, 0.0f);
        tmp.assign (kAna, 0.0f);
        cmnd.assign (512, 1.0f);
        tauMin = std::max (2, (int) (effSr / 1000.0));
        tauMax = std::min (480, (int) (effSr / 70.0));
        smooth = 1.0f - (float) std::exp (-1.0 / (0.012 * sr)); // ~12 ms retune time = hard tune
        reset();
    }

    void reset()
    {
        for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f);
        std::fill (ana.begin(), ana.end(), 0.0f);
        w = anaPos = accCount = hopCount = 0;
        acc = 0.0f; phase = 0.0f; ratio = targetRatio = 1.0f;
    }

    void process (float& l, float& r)
    {
        // --- analysis (decimated mono)
        acc += 0.5f * (l + r);
        if (++accCount >= dec)
        {
            ana[(size_t) anaPos] = acc / (float) dec;
            anaPos = (anaPos + 1) & kAnaMask;
            acc = 0.0f; accCount = 0;
            if (++hopCount >= 128) { hopCount = 0; detect(); }
        }

        ratio += (targetRatio - ratio) * smooth;

        buf[0][(size_t) w] = l;
        buf[1][(size_t) w] = r;

        phase += (1.0f - ratio) / (float) winLen;
        phase -= std::floor (phase);
        float p2 = phase + 0.5f;
        p2 -= std::floor (p2);

        const float d1 = phase * (float) winLen, d2 = p2 * (float) winLen;
        const float g1 = 0.5f - 0.5f * std::cos ((float) (2.0 * kPi) * phase);
        const float g2 = 0.5f - 0.5f * std::cos ((float) (2.0 * kPi) * p2);

        l = g1 * readFrac (0, d1) + g2 * readFrac (0, d2);
        r = g1 * readFrac (1, d1) + g2 * readFrac (1, d2);

        w = (w + 1) & kBufMask;
    }

    float getRatio() const { return ratio; }

private:
    static constexpr int kBuf = 8192, kBufMask = kBuf - 1;
    static constexpr int kAna = 1024, kAnaMask = kAna - 1;

    float readFrac (int ch, float d) const
    {
        const float pos = (float) w - d;
        const int i0 = (int) std::floor (pos);
        const float f = pos - (float) i0;
        const int a = i0 & kBufMask, b = (i0 + 1) & kBufMask;
        return buf[ch][(size_t) a] * (1.0f - f) + buf[ch][(size_t) b] * f;
    }

    void detect()
    {
        const int W = 512;
        const int total = W + tauMax;

        for (int i = 0; i < total; ++i)
            tmp[(size_t) i] = ana[(size_t) ((anaPos - total + i) & kAnaMask)];

        float e = 0.0f;
        for (int i = 0; i < W; ++i) { const float v = tmp[(size_t) (total - W + i)]; e += v * v; }
        if (std::sqrt (e / (float) W) < 0.005f) { targetRatio = 1.0f; return; } // gate (unvoiced / silence)

        float running = 0.0f;
        cmnd[0] = 1.0f;
        for (int tau = 1; tau < tauMax; ++tau)
        {
            float sum = 0.0f;
            for (int j = 0; j < W; ++j)
            {
                const float d = tmp[(size_t) j] - tmp[(size_t) (j + tau)];
                sum += d * d;
            }
            running += sum;
            cmnd[(size_t) tau] = running > 0.0f ? sum * (float) tau / running : 1.0f;
        }

        int est = -1;
        for (int tau = tauMin; tau < tauMax - 1; ++tau)
        {
            if (cmnd[(size_t) tau] < 0.15f)
            {
                while (tau + 1 < tauMax - 1 && cmnd[(size_t) (tau + 1)] < cmnd[(size_t) tau]) ++tau;
                est = tau;
                break;
            }
        }

        if (est < 0) { targetRatio = 1.0f; return; }

        const float a = cmnd[(size_t) (est - 1)], b = cmnd[(size_t) est], c = cmnd[(size_t) (est + 1)];
        const float den = a - 2.0f * b + c;
        const float shift = den != 0.0f ? 0.5f * (a - c) / den : 0.0f;
        const double freq = effSr / ((double) est + (double) shift);

        const double midi = 69.0 + 12.0 * std::log2 (freq / 440.0);
        const double diff = std::round (midi) - midi; // semitones to move (max +-0.5)
        targetRatio = (float) std::pow (2.0, diff / 12.0);
    }

    std::vector<float> buf[2], ana, tmp, cmnd;
    int dec = 3, winLen = 1400, tauMin = 16, tauMax = 210;
    int w = 0, anaPos = 0, accCount = 0, hopCount = 0;
    double effSr = 16000.0;
    float acc = 0.0f, phase = 0.0f, ratio = 1.0f, targetRatio = 1.0f, smooth = 0.002f;
};
} // namespace vv
