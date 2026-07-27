// ============================================================================
// BackoffScheduler.h — High-precision random backoff delay scheduler
// ============================================================================
// Provides a spin-wait delay mechanism using QueryPerformanceCounter for
// microsecond-level precision.  Random delay range: 5000 µs ~ 50000 µs
// (5 ms ~ 50 ms).  Designed to smooth out polling peaks in cross-process
// memory access loops.  No Sleep() calls — pure spin-wait.
// ============================================================================
#pragma once

#include <windows.h>
#include <cstdint>

// ---------------------------------------------------------------------------
// BackoffScheduler — per-thread or per-context scheduler instance
//
// Usage:
//   BackoffScheduler sched(5000, 50000);  // 5–50 ms range
//   while (polling) {
//       // ... do work ...
//       sched.Wait();   // spin-waits for a random time in [min, max]
//   }
// ---------------------------------------------------------------------------
class BackoffScheduler {
public:
    // -----------------------------------------------------------------------
    // Construct with explicit min/max delay in microseconds.
    //   minDelayUs  — minimum delay in microseconds (default 5000 = 5 ms)
    //   maxDelayUs  — maximum delay in microseconds (default 50000 = 50 ms)
    // -----------------------------------------------------------------------
    BackoffScheduler(uint32_t minDelayUs = 5000, uint32_t maxDelayUs = 50000);

    // -----------------------------------------------------------------------
    // Wait — spin-waits for a random duration in [minDelayUs, maxDelayUs].
    // Uses QueryPerformanceCounter for microsecond precision.
    // Never calls Sleep() — pure spin-loop with _mm_pause() on x64.
    // -----------------------------------------------------------------------
    void Wait();

    // -----------------------------------------------------------------------
    // WaitEx — same as Wait() but returns the actual elapsed time in
    // microseconds (measured via QPC).  Useful for diagnostics.
    // -----------------------------------------------------------------------
    uint64_t WaitEx();

    // -----------------------------------------------------------------------
    // Reset — re-seeds the internal RNG with the current QPC value.
    // -----------------------------------------------------------------------
    void Reset();

    // -----------------------------------------------------------------------
    // Getters
    // -----------------------------------------------------------------------
    uint32_t GetMinDelayUs() const { return m_minDelayUs; }
    uint32_t GetMaxDelayUs() const { return m_maxDelayUs; }

    // -----------------------------------------------------------------------
    // Set new delay bounds (both clamped to 100–1000000 µs range).
    // -----------------------------------------------------------------------
    void SetRange(uint32_t minUs, uint32_t maxUs);

private:
    // -----------------------------------------------------------------------
    // Simple xorshift64* PRNG (stateless per instance, no global state).
    // -----------------------------------------------------------------------
    uint64_t NextRand();

    uint64_t    m_state;
    uint32_t    m_minDelayUs;
    uint32_t    m_maxDelayUs;
    double      m_qpcFreqInv;       // 1.0 / frequency, for µs conversion

    // Cached frequency (read once from QueryPerformanceFrequency).
    static double GetQpcInvFrequency();
};

// ---------------------------------------------------------------------------
// Convenience: static singleton for global "one-off" delays.
// Thread-safe for single-threaded use.  For multi-threaded scenarios,
// create per-thread instances.
// ---------------------------------------------------------------------------
BackoffScheduler& GetDefaultBackoffScheduler();
