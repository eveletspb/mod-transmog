#ifndef MOD_TRANSMOG_PRICE_H
#define MOD_TRANSMOG_PRICE_H

#include <cmath>
#include <cstdint>
#include <limits>

inline uint32_t TransmogCost(uint64_t base, float modifier, int32_t extra)
{
    double value = static_cast<double>(base) * modifier + extra;
    if (!std::isfinite(value) || value >= std::numeric_limits<int32_t>::max())
        return std::numeric_limits<int32_t>::max();
    return value > 0 ? static_cast<uint32_t>(value) : 0;
}

#endif
