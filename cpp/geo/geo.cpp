#include "gommanna/contracts.hpp"

extern "C" {
#include "../../GomManna/Services/CRules/Include/distance.h"
}

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace gommanna {
namespace {
bool valid_coordinate(Coordinate c) {
    return std::isfinite(c.latitude) && std::isfinite(c.longitude) &&
           std::abs(c.latitude) <= 90 && std::abs(c.longitude) <= 180;
}
}

Result<TravelEstimate> estimate_walk(Coordinate from, Coordinate to, double speed) {
    if (!valid_coordinate(from) || !valid_coordinate(to) || !std::isfinite(speed) || speed <= 0)
        return Error{ErrorCode::invalid_argument, "invalid coordinate or walking speed"};
    const auto distance = haversine_distance_m(from.latitude, from.longitude, to.latitude, to.longitude);
    const auto minutes = std::ceil(distance / speed);
    if (!std::isfinite(minutes) || minutes >= static_cast<double>(std::numeric_limits<std::int64_t>::max()))
        return Error{ErrorCode::invalid_argument, "walking estimate is out of range"};
    return TravelEstimate{distance, static_cast<std::int64_t>(minutes), speed};
}

Freshness classify_location(const std::optional<Location>& location, bool sharing, Epoch epoch, TimeMs now) {
    if (!sharing) return Freshness::off;
    if (!location || location->sharing_epoch != epoch || location->captured_at_ms < 0 ||
        static_cast<long double>(location->captured_at_ms) > static_cast<long double>(now) + kFutureToleranceMs ||
        !valid_coordinate(location->coordinate)) return Freshness::no_data;
    const auto age = std::max<TimeMs>(0, now - location->captured_at_ms);
    if (age < kDelayedAfterMs) return Freshness::fresh;
    if (age < kStaleAfterMs) return Freshness::delayed;
    return Freshness::stale;
}
} // namespace gommanna
