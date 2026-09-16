#pragma once

#include <SDL2/SDL.h>
#include <algorithm>

namespace monolith::detail {

// SDL tick values wrap at 2^32. This helper is for short-lived deadlines whose
// lifetime is less than half that range.
inline bool tickDeadlinePending(Uint32 now, Uint32 deadline) {
    const Uint32 remaining = deadline - now;
    return remaining != 0u && remaining < 0x80000000u;
}

// Convert a short frame interval to seconds while keeping SDL's wrapping
// 32-bit tick counter and long stalls inside one shared game-timing contract.
inline float tickDeltaSeconds(Uint32 now, Uint32 previous, float maxSeconds = 0.05f) {
    const float elapsed = static_cast<float>(now - previous) / 1000.f;
    return std::clamp(elapsed, 0.f, maxSeconds);
}

} // namespace monolith::detail
