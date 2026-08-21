#pragma once

#include <vector>
#include <cstdint>

// Immutable Run-Length encoding vec. Allows pushing and reading but not writing
template <typename T>
class RLEVec
{
public:
    void Push(T v)
    {
        if (!value.empty() && value.back() == v)
        {
            ++lengthEnd.back();
            return;
        }

        const size_t previousEnd = lengthEnd.empty() ? 0 : lengthEnd.back();

        value.push_back(v);
        lengthEnd.push_back(previousEnd + 1);
    }

    const bool Empty() const
    {
        return value.empty();
    }

    size_t Size() const
    {
        return lengthEnd.empty() ? 0 : lengthEnd.back();
    }

    void Clear()
    {
        lengthEnd.clear();
        value.clear();
        rleIndex = 0;
    }

    void Fill(const T& v)
    {
        size_t currSize = Size();
        Clear();
        lengthEnd.push_back(currSize);
        value.push_back(v);
    }

    const T& operator[](size_t index) const
    {
        auto it = std::upper_bound(
            lengthEnd.begin(),
            lengthEnd.end(),
            index
        );

        return value[it - lengthEnd.begin()];
    }

    void Set(size_t index, const T& v)
    {
        if (index >= Size()) return;

        size_t run =
            static_cast<size_t>(std::upper_bound(lengthEnd.begin(), lengthEnd.end(), index) - lengthEnd.begin());

        if (value[run] == v) return;

        const size_t runStart = run == 0 ? 0 : lengthEnd[run - 1];
        const size_t runEnd = lengthEnd[run];
        const bool hasLeft = index > runStart;
        const bool hasRight = index + 1 < runEnd;
        const T oldValue = value[run];

        if (!hasLeft && !hasRight)
        {
            value[run] = v;

            if (run > 0 && value[run - 1] == v)
            {
                lengthEnd[run - 1] = lengthEnd[run];
                value.erase(value.begin() + run);
                lengthEnd.erase(lengthEnd.begin() + run);
                --run;
            }
            if (run + 1 < value.size() && value[run + 1] == v)
            {
                lengthEnd[run] = lengthEnd[run + 1];
                value.erase(value.begin() + run + 1);
                lengthEnd.erase(lengthEnd.begin() + run + 1);
            }
        }
        else if (hasLeft && !hasRight)
        {
            lengthEnd[run] = index;
            const bool mergeRight = (run + 1 < value.size()) && value[run + 1] == v;
            if (!mergeRight)
            {
                value.insert(value.begin() + run + 1, v);
                lengthEnd.insert(lengthEnd.begin() + run + 1, runEnd);
            }
        }
        else if (!hasLeft && hasRight)
        {
            const bool mergeLeft = (run > 0) && value[run - 1] == v;
            if (mergeLeft)
            {
                lengthEnd[run - 1] = index + 1;
            }
            else
            {
                value.insert(value.begin() + run, v);
                lengthEnd.insert(lengthEnd.begin() + run, index + 1);
            }
        }
        else
        {
            lengthEnd[run] = index;
            value.insert(value.begin() + run + 1, { v, oldValue });
            lengthEnd.insert(lengthEnd.begin() + run + 1, { index + 1, runEnd });
        }

        rleIndex = 0;
    }

private:
    std::vector<size_t> lengthEnd{};
    std::vector<T> value{};

    mutable size_t rleIndex = 0;
};