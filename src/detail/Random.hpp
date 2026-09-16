#pragma once

#include <cstdint>
#include <random>

namespace monolith::detail {

// Keep each game's random stream local so one app cannot alter another app's
// board or food sequence.
class Random {
public:
    Random()
        : m_engine(std::random_device{}()) {}

    explicit Random(std::uint32_t seed)
        : m_engine(seed) {}

    int uniformInt(int upperExclusive) {
        if (upperExclusive <= 1) return 0;
        return std::uniform_int_distribution<int>(0, upperExclusive - 1)(m_engine);
    }

private:
    std::mt19937 m_engine;
};

} // namespace monolith::detail
