#pragma once

// Per-key acceleration structure for "which notes can own a pixel row".
// Notes of one key must be sorted by start tick.
// Built once at load; queries use monotonic start cursors and block-pruned end enumeration.

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>
#include <bit>
#include "Concurrency/ThreadPool.h"
#include "Utils.h"

struct RowQuery
{
    double tNow;
    double ticksPerPixel;
    double yBase;
    int rowBegin;
    int rowEnd;
    double marginPx;
    size_t startCap;

    uint32_t minGate = 0; // smallest gate that a note can be before the key index starts ignoring it.
    double minVisualPx = 0;  // smallest gate that a note renders as. used for synthesia renderer
    bool pixelSnap = false;
};

class KeyNoteIndex
{
public:
    static constexpr size_t B = 32;

    void Build(const uint32_t* tick, const uint32_t* gate, size_t count, bool withEndTable = true)
    {
        tick_ = tick;
        gate_ = gate;
        n_ = count;

        nb_ = (n_ + B - 1) / B;
        size_ = 1;
        while (size_ < nb_)
            size_ <<= 1;

        tree_.assign(2 * size_, 0);

        for (size_t i = 0; i < n_; ++i)
        {
            const uint64_t e = End(i);
            uint64_t& leaf = tree_[size_ + i / B];
            leaf = std::max(leaf, e);
        }

        for (size_t p = size_; p > 1;)
        {
            --p;
            tree_[p] = std::max(tree_[p * 2], tree_[p * 2 + 1]);
        }

        sortedEnds_.clear();
        if (withEndTable)
        {
            sortedEnds_.resize(n_);
            for (size_t i = 0; i < n_; ++i)
                sortedEnds_[i] = End(i);

            std::sort(sortedEnds_.begin(), sortedEnds_.end());
        }

        owners_.Reserve(n_);
    }

    // Number of notes with end <= t. Requires Build(..., withEndTable=true).
    size_t CountEndAtOrBefore(double t) const
    {
        return static_cast<size_t>(
            std::upper_bound(sortedEnds_.begin(), sortedEnds_.end(), t,
                [](double value, uint64_t end)
                {
                    return value < static_cast<double>(end);
                }) - sortedEnds_.begin());
    }

    // Index of the last-drawn note sounding at t (start <= t < end), or -1.
    ptrdiff_t LastActiveAt(double t) const
    {
        const size_t c = CountStartAtOrBefore(t);
        return c
            ? RightmostEndAtLeast(static_cast<ptrdiff_t>(c) - 1,
                std::nextafter(t, std::numeric_limits<double>::infinity()))
            : -1;
    }

    size_t Size() const { return n_; }

    uint64_t End(size_t i) const
    {
        return static_cast<uint64_t>(tick_[i]) + gate_[i];
    }

    // Number of notes with start < t.
    size_t CountStartBefore(double t) const
    {
        size_t lo = 0;
        size_t hi = n_;

        while (lo < hi)
        {
            const size_t mid = lo + (hi - lo) / 2;
            if (static_cast<double>(tick_[mid]) < t)
                lo = mid + 1;
            else
                hi = mid;
        }

        return lo;
    }

    // Number of notes with start <= t.
    size_t CountStartAtOrBefore(double t) const
    {
        size_t lo = 0;
        size_t hi = n_;

        while (lo < hi)
        {
            const size_t mid = lo + (hi - lo) / 2;
            if (static_cast<double>(tick_[mid]) <= t)
                lo = mid + 1;
            else
                hi = mid;
        }

        return lo;
    }

    // Largest index <= iMax whose end >= x, or -1.
    ptrdiff_t RightmostEndAtLeast(ptrdiff_t iMax, double x, double m = 0.0) const
    {
        if (iMax < 0 || n_ == 0) return -1;
        iMax = std::min<ptrdiff_t>(iMax, (ptrdiff_t)n_ - 1);
        if (m > 0.0 && (double)tick_[iMax] >= x - m) return iMax;

        const size_t maxIndex = std::min(static_cast<size_t>(iMax), n_ - 1);
        const size_t block = maxIndex / B;
        const size_t blockStart = block * B;

        // The rightmost candidate may be in the partial final block.
        for (ptrdiff_t i = static_cast<ptrdiff_t>(maxIndex);
            i >= static_cast<ptrdiff_t>(blockStart); --i)
        {
            if (static_cast<double>(End(static_cast<size_t>(i))) >= x)
                return i;
        }

        if (block == 0)
            return -1;

        const ptrdiff_t foundBlock =
            LastBlockAtLeast(static_cast<ptrdiff_t>(block) - 1, x);

        if (foundBlock < 0)
            return -1;

        const size_t lo = static_cast<size_t>(foundBlock) * B;
        const size_t hi = std::min(n_, lo + B);

        for (ptrdiff_t i = static_cast<ptrdiff_t>(hi) - 1;
            i >= static_cast<ptrdiff_t>(lo); --i)
        {
            if (static_cast<double>(End(static_cast<size_t>(i))) >= x)
                return i;
        }

        return -1;
    }

    // Single-threaded overload remains available for small jobs / callers
    // that do not have access to the application's worker pool.
    void CollectRowOwners(std::vector<uint32_t>& out, const RowQuery& q) const
    {
        out.clear();

        if (n_ == 0 || q.rowBegin >= q.rowEnd ||
            q.ticksPerPixel <= 0.0 || size_ == 0)
            return;

        CollectRowOwnersRange(owners_, q, q.rowBegin, q.rowEnd);
        owners_.Drain(out);
    }

    // Notes with start < startCap whose end > tNow (i.e. potentially visible).
    size_t CountVisible(double tNow, size_t startCap, size_t ended) const
    {
        return startCap > ended ? startCap - ended : 0;
    }

    // Every potentially visible note, ascending. Uses the segment tree, so it costs ~O(k log n).
    void CollectVisible(std::vector<uint32_t>& out, double tNow,
        size_t startCap, uint32_t minGate = 0, double m = 0.0) const
    {
        out.clear();
        startCap = std::min(startCap, n_);
        if (startCap == 0 || size_ == 0)
            return;

        const double x = std::nextafter(tNow, std::numeric_limits<double>::infinity());
        CollectRangeAtLeast(out, 0, startCap - 1, x, minGate, m);   // yields descending
        std::reverse(out.begin(), out.end());
    }
private:
    static double Up(double v) { return std::nextafter(v, std::numeric_limits<double>::infinity()); }
    static double SnapRound(double v) { return Utils::RoundEven(v); }

    // Pixel rows [top, bottom) that note i is drawn into when snapped.
    void SnappedRows(size_t i, const RowQuery& q, double& top, double& bottom) const
    {
        const double inv = 1.0 / q.ticksPerPixel;
        bottom = SnapRound(q.yBase - (static_cast<double>(tick_[i]) - q.tNow) * inv);
        top = bottom - SnapRound(static_cast<double>(gate_[i]) * inv);
    }

    struct OwnerSet
    {
        std::vector<uint64_t> bits;
        size_t lo = SIZE_MAX, hi = 0;

        void Reserve(size_t n) { bits.assign((n + 63) / 64, 0); lo = SIZE_MAX; hi = 0; }

        void push_back(uint32_t i)               // same name so the existing code works unchanged
        {
            bits[i >> 6] |= 1ull << (i & 63);
            lo = std::min<size_t>(lo, i);
            hi = std::max<size_t>(hi, i);
        }

        void Drain(std::vector<uint32_t>& out)   // ascending, unique, and clears itself
        {
            out.clear();
            if (lo > hi) return;
            for (size_t w = lo >> 6; w <= (hi >> 6); ++w)
            {
                uint64_t word = bits[w];
                bits[w] = 0;
                while (word)
                {
                    out.push_back(static_cast<uint32_t>((w << 6) + std::countr_zero(word)));
                    word &= word - 1;
                }
            }
            lo = SIZE_MAX; hi = 0;
        }
    };
    mutable OwnerSet owners_;

    static void SortUnique(std::vector<uint32_t>& out)
    {
        std::sort(out.begin(), out.end());
        out.erase(std::unique(out.begin(), out.end()), out.end());
    }

    // Processes one contiguous row band. Safe to run concurrently with other
    // bands because this index is read-only and each task has its own output.
    template <class Out>
    void CollectRowOwnersRange(Out& out, const RowQuery& q,
        int rowBegin, int rowEnd) const
    {
        const double margin = q.marginPx * q.ticksPerPixel;
        const size_t cap = std::min(q.startCap, n_);
        const double m = q.minVisualPx * q.ticksPerPixel;

        if (cap == 0)
            return;

        // Conservative slack (ticks) so the superset search never misses a
        // note whose snapped extent reaches the row. Zero when not snapping.
        constexpr double kEps = 1e-6;
        const double sp = q.pixelSnap ? (1.0 + kEps) * q.ticksPerPixel : 0.0; // start side
        const double ep = q.pixelSnap ? (0.5 + kEps) * q.ticksPerPixel : 0.0; // end side

        const double endFloor = std::nextafter(q.tNow, std::numeric_limits<double>::infinity());

        std::vector<uint32_t> scratch; // snapped path only; reused across rows

        bool firstRow = true;
        size_t cL = 0;
        size_t cS = 0;

        for (int r = rowEnd - 1; r >= rowBegin; --r)
        {
            const double t =
                q.tNow + (q.yBase - (static_cast<double>(r) + 0.5))
                * q.ticksPerPixel;

            const double startHi = t + margin;
            const double startLo = t - margin;
            // Half-open [start, end): start <= t  <=>  start < nextafter(t)
            const double cntHi = Up(startHi + sp);
            const double cntLo = Up(startLo + sp);

            if (firstRow)
            {
                cL = std::min(cap, CountStartBefore(cntHi));
                cS = std::min(cap, CountStartBefore(cntLo));
                firstRow = false;
            }
            else
            {
                cL = GallopStartBefore(cL, cap, cntHi);
                cS = GallopStartBefore(cS, cap, cntLo);
            }

            if (cL == 0)
                continue;

            // end > t  <=>  end >= nextafter(t)
            const double xLo = std::max(Up(startLo - ep), endFloor);
            const double xHi = std::max(Up(startHi - ep), endFloor);

            const ptrdiff_t iL =
                RightmostEndAtLeast(static_cast<ptrdiff_t>(cL) - 1, xLo, m);
            if (iL < 0)
                continue;

            if (!q.pixelSnap)
            {
                const ptrdiff_t iS = cS
                    ? RightmostEndAtLeast(static_cast<ptrdiff_t>(cS) - 1, xHi, m)
                    : -1;

                if (iL > iS)
                    CollectRangeAtLeast(out, static_cast<size_t>(iS + 1),
                        static_cast<size_t>(iL), xLo, q.minGate, m);

                if (iS >= 0 && gate_[static_cast<size_t>(iS)] >= q.minGate)
                    out.push_back(static_cast<uint32_t>(iS));
                continue;
            }

            // ---- pixel-snapped path: exact test on the superset ----
            const double c = static_cast<double>(r) + 0.5;
            const double wLo = c - q.marginPx;
            const double wHi = c + q.marginPx;

            // Rightmost note whose *snapped* extent fully covers the window.
            // The superset search can return false positives near the rounding
            // boundary, so step left until one passes the exact test.
            ptrdiff_t iS = cS
                ? RightmostEndAtLeast(static_cast<ptrdiff_t>(cS) - 1, xHi, m)
                : -1;
            while (iS >= 0)
            {
                double top, bot;
                SnappedRows(static_cast<size_t>(iS), q, top, bot);
                if (top <= wLo && bot >= wHi)
                    break;
                iS = iS > 0 ? RightmostEndAtLeast(iS - 1, xHi, m) : -1;
            }

            if (iL > iS)
            {
                scratch.clear();
                CollectRangeAtLeast(scratch, static_cast<size_t>(iS + 1),
                    static_cast<size_t>(iL), xLo, q.minGate, m);

                for (const uint32_t i : scratch)
                {
                    double top, bot;
                    SnappedRows(i, q, top, bot);
                    if (top < wHi && bot > wLo)
                        out.push_back(i);
                }
            }

            if (iS >= 0 && gate_[static_cast<size_t>(iS)] >= q.minGate)
                out.push_back(static_cast<uint32_t>(iS));
        }
    }

    // Enumerate qualifying notes in [indexLo, indexHi], descending.
    // Segment-tree nodes whose maximum end is below x are skipped wholesale.
    template <class Out>
    void CollectRangeAtLeast(Out& out,
        size_t indexLo, size_t indexHi,
        double x, uint32_t minGate, double m = 0.0) const
    {
        if (indexLo > indexHi || indexLo >= n_) return;
        indexHi = std::min(indexHi, n_ - 1);

        const size_t s0 = m > 0.0 ? CountStartBefore(x - m) : indexHi + 1;
        for (size_t i = indexHi + 1; i > std::max(indexLo, s0);)
        {
            --i;
            if (gate_[i] >= minGate) out.push_back((uint32_t)i);
        }

        const size_t firstBlock = indexLo / B;
        const size_t lastBlock = indexHi / B;

        if (s0 > indexLo)
            CollectBlocksAtLeast(out, 1, 0, size_,
                indexLo / B, std::min(indexHi, s0 - 1) / B,
                indexLo, std::min(indexHi, s0 - 1),
                x, minGate);
    }

    template <class Out>
    void CollectBlocksAtLeast(Out& out,
        size_t node,
        size_t nodeBegin, size_t nodeEnd,
        size_t queryBegin, size_t queryEnd,
        size_t indexLo, size_t indexHi,
        double x, uint32_t minGate) const
    {
        if (nodeEnd <= queryBegin || nodeBegin > queryEnd)
            return;

        if (static_cast<double>(tree_[node]) < x)
            return;

        if (nodeEnd - nodeBegin == 1)
        {
            const size_t blockBegin = nodeBegin * B;
            const size_t lo = std::max(indexLo, blockBegin);
            const size_t hi = std::min(indexHi + 1,
                std::min(n_, blockBegin + B));

            for (size_t i = hi; i > lo;)
            {
                --i;
                if (static_cast<double>(End(i)) >= x && gate_[i] >= minGate)
                    out.push_back(static_cast<uint32_t>(i));
            }

            return;
        }

        const size_t mid = nodeBegin + (nodeEnd - nodeBegin) / 2;

        // Right first: matches the original rightmost-to-leftmost enumeration.
        CollectBlocksAtLeast(out, node * 2 + 1, mid, nodeEnd,
            queryBegin, queryEnd, indexLo, indexHi, x, minGate);

        CollectBlocksAtLeast(out, node * 2, nodeBegin, mid,
            queryBegin, queryEnd, indexLo, indexHi, x, minGate);
    }

    // Rightmost block <= hi whose maximum end >= x.
    ptrdiff_t LastBlockAtLeast(ptrdiff_t hi, double x) const
    {
        if (hi < 0 || size_ == 0)
            return -1;

        size_t p = size_ + static_cast<size_t>(hi);

        if (static_cast<double>(tree_[p]) >= x)
            return hi;

        while (p > 1)
        {
            if ((p & 1) && static_cast<double>(tree_[p - 1]) >= x)
            {
                --p;

                while (p < size_)
                {
                    const size_t right = p * 2 + 1;
                    p = static_cast<double>(tree_[right]) >= x
                        ? right
                        : p * 2;
                }

                return static_cast<ptrdiff_t>(p - size_);
            }

            p >>= 1;
        }

        return -1;
    }

    // First index i in [from, cap] with tick_[i] >= t.
    // Requires every index < from to already have tick_ < t (true for your monotonic cursors).
    size_t GallopStartBefore(size_t from, size_t cap, double t) const
    {
        if (from >= cap || static_cast<double>(tick_[from]) >= t)
            return from;

        // Invariant from here on: tick_[lo] < t.
        size_t lo = from;
        size_t step = 1;
        size_t hi;

        // Phase 1: double the stride until we overshoot (or hit cap).
        for (;;)
        {
            hi = std::min(lo + step, cap);
            if (hi == cap || static_cast<double>(tick_[hi]) >= t)
                break;
            lo = hi;
            step <<= 1;
        }

        // Phase 2: binary search in (lo, hi]. The answer is at most hi.
        while (lo + 1 < hi)
        {
            const size_t mid = lo + (hi - lo) / 2;
            if (static_cast<double>(tick_[mid]) < t)
                lo = mid;
            else
                hi = mid;
        }
        return hi;
    }

    const uint32_t* tick_ = nullptr;
    const uint32_t* gate_ = nullptr;

    size_t n_ = 0;
    size_t nb_ = 0;
    size_t size_ = 1;

    std::vector<uint64_t> tree_;
    std::vector<uint64_t> sortedEnds_;
};