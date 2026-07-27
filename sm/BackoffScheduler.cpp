// ============================================================================
// BackoffScheduler.cpp — High-precision random backoff implementation
// ============================================================================
#include "BackoffScheduler.h"
#include <cstdlib>
#include <intrin.h>  // for _mm_pause

// ---------------------------------------------------------------------------
// Constructor: seeds RNG from QPC and caches frequency inverse
// ---------------------------------------------------------------------------
BackoffScheduler::BackoffScheduler(uint32_t minDelayUs, uint32_t maxDelayUs)
    : m_minDelayUs(minDelayUs)
    , m_maxDelayUs(maxDelayUs)
    , m_qpcFreqInv(GetQpcInvFrequency())
{
    // Seed with QPC value + address of this instance for per-instance uniqueness
    LARGE_INTEGER qpc;
    QueryPerformanceCounter(&qpc);
    m_state = static_cast<uint64_t>(qpc.QuadPart) ^
              reinterpret_cast<uint64_t>(this) ^
              0x9E3779B97F4A7C15ULL;  // golden ratio
    if (m_state == 0) m_state = 1;

    // Clamp bounds
    if (m_minDelayUs < 100)   m_minDelayUs = 100;
    if (m_maxDelayUs > 1000000) m_maxDelayUs = 1000000;
    if (m_minDelayUs > m_maxDelayUs) {
        uint32_t t = m_minDelayUs;
        m_minDelayUs = m_maxDelayUs;
        m_maxDelayUs = t;
    }
}

// ---------------------------------------------------------------------------
// GetQpcInvFrequency — read QPC frequency once, return its inverse as double
// ---------------------------------------------------------------------------
double BackoffScheduler::GetQpcInvFrequency() {
    static double s_invFreq = 0.0;
    if (s_invFreq == 0.0) {
        LARGE_INTEGER freq;
        if (QueryPerformanceFrequency(&freq) && freq.QuadPart != 0) {
            s_invFreq = 1.0 / static_cast<double>(freq.QuadPart);
        } else {
            // Fallback: assume 10 MHz (common TSC-based QPC)
            s_invFreq = 1.0 / 10000000.0;
        }
    }
    return s_invFreq;
}

// ---------------------------------------------------------------------------
// xorshift64* — fast, seedable PRNG
// ---------------------------------------------------------------------------
uint64_t BackoffScheduler::NextRand() {
    uint64_t x = m_state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    m_state = x;
    return x * 0x2545F4914F6CDD1DULL;
}

// ---------------------------------------------------------------------------
// Wait — spin-wait for a random duration in [m_minDelayUs, m_maxDelayUs].
// ---------------------------------------------------------------------------
void BackoffScheduler::Wait() {
    WaitEx();
}

// ---------------------------------------------------------------------------
// WaitEx — spin-wait and return actual elapsed microseconds
// ---------------------------------------------------------------------------
uint64_t BackoffScheduler::WaitEx() {
    // Generate target delay in microseconds
    uint64_t range = static_cast<uint64_t>(m_maxDelayUs) - static_cast<uint64_t>(m_minDelayUs);
    uint64_t delayUs = m_minDelayUs;
    if (range > 0) {
        delayUs += static_cast<uint32_t>(NextRand() % (range + 1));
    }

    // Convert to a target QPC count
    double targetDelta = static_cast<double>(delayUs) / 1'000'000.0;  // seconds
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    double startSec = static_cast<double>(now.QuadPart) * m_qpcFreqInv;
    double endSec = startSec + targetDelta;

    // Spin-wait with _mm_pause() for power efficiency
    LARGE_INTEGER current;
    do {
        // _mm_pause() hints the CPU that we are in a spin-loop, improving
        // power efficiency and hyper-threading fairness.
        _mm_pause();
        QueryPerformanceCounter(&current);
        double currentSec = static_cast<double>(current.QuadPart) * m_qpcFreqInv;
        if (currentSec >= endSec) break;
    } while (true);

    // Return actual measured elapsed time
    double elapsedSec = static_cast<double>(current.QuadPart) * m_qpcFreqInv - startSec;
    uint64_t elapsedUs = static_cast<uint64_t>(elapsedSec * 1'000'000.0 + 0.5);
    return elapsedUs;
}

// ---------------------------------------------------------------------------
// Reset — re-seed the RNG
// ---------------------------------------------------------------------------
void BackoffScheduler::Reset() {
    LARGE_INTEGER qpc;
    QueryPerformanceCounter(&qpc);
    m_state = static_cast<uint64_t>(qpc.QuadPart) ^ 0x9E3779B97F4A7C15ULL;
    if (m_state == 0) m_state = 1;
}

// ---------------------------------------------------------------------------
// SetRange — update min/max delay bounds
// ---------------------------------------------------------------------------
void BackoffScheduler::SetRange(uint32_t minUs, uint32_t maxUs) {
    if (minUs < 100) minUs = 100;
    if (maxUs > 1000000) maxUs = 1000000;
    if (minUs > maxUs) { uint32_t t = minUs; minUs = maxUs; maxUs = t; }
    m_minDelayUs = minUs;
    m_maxDelayUs = maxUs;
}

// ---------------------------------------------------------------------------
// Default singleton
// ---------------------------------------------------------------------------
BackoffScheduler& GetDefaultBackoffScheduler() {
    static BackoffScheduler s_default(5000, 50000);
    return s_default;
}
