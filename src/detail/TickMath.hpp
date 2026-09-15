#pragma once

#include <SDL2/SDL.h>

namespace monolith::detail {

// SDL tick values wrap at 2^32. This helper is for short-lived deadlines whose
// lifetime is less than half that range.
inline bool tickDeadlinePending(Uint32 now, Uint32 deadline) {
    const Uint32 remaining = deadline - now;
    return remaining != 0u && remaining < 0x80000000u;
}

} // namespace monolith::detail
