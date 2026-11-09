#pragma once
#include "contracts.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <string_view>

namespace gommanna {
inline TimeMs guest_session_expiry(TimeMs now) {
    using namespace std::chrono;
    const sys_time<milliseconds> point{milliseconds{now}};
    const auto date = floor<days>(point);
    const year_month_day start{date};
    const auto target_month = start.year()/start.month() + months{6};
    const auto last_day = year_month_day_last{target_month/last}.day();
    const year_month_day target{target_month/std::min(start.day(),last_day)};
    return duration_cast<milliseconds>((sys_days{target} + (point-date)).time_since_epoch()).count();
}
inline bool valid_animal_icon(std::string_view icon) {
    constexpr std::array<std::string_view,9> icons{
        "bear","rabbit","fox","cat","dog","panda","koala","tiger","penguin"};
    return std::find(icons.begin(),icons.end(),icon)!=icons.end();
}
inline double route_distance(Coordinate a, Coordinate b) {
    constexpr double radians=3.14159265358979323846/180.0;
    const double lat=(b.latitude-a.latitude)*radians, lon=(b.longitude-a.longitude)*radians;
    const double h=std::pow(std::sin(lat/2),2)+std::cos(a.latitude*radians)*
        std::cos(b.latitude*radians)*std::pow(std::sin(lon/2),2);
    return 2*kEarthRadiusM*std::asin(std::sqrt(std::clamp(h,0.0,1.0)));
}
inline bool should_store_route(const std::optional<Location>& last, const Location& next) {
    return !last || last->sharing_epoch!=next.sharing_epoch ||
        next.received_at_ms-last->received_at_ms>=kRouteHeartbeatMs ||
        route_distance(last->coordinate,next.coordinate)>=kRouteMinimumDistanceM;
}
}
