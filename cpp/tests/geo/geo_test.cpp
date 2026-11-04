#include "gommanna/contracts.hpp"

#include <cmath>
#include <iostream>
#include <limits>

using namespace gommanna;
#define CHECK(test) do { if (!(test)) { std::cerr << "FAIL line " << __LINE__ << ": " #test "\n"; return 1; } } while (false)

int main() {
    auto same=estimate_walk({37.5,127.0},{37.5,127.0});
    CHECK(std::holds_alternative<TravelEstimate>(same));
    CHECK(std::get<TravelEstimate>(same).distance_m==0 && std::get<TravelEstimate>(same).walk_minutes==0);
    auto normal=estimate_walk({0,0},{0,0.001});
    CHECK(std::holds_alternative<TravelEstimate>(normal));
    CHECK(std::abs(std::get<TravelEstimate>(normal).distance_m-111.195)<0.1);
    CHECK(std::get<TravelEstimate>(normal).walk_minutes==2);
    CHECK(std::holds_alternative<Error>(estimate_walk({91,0},{0,0})));
    CHECK(std::holds_alternative<Error>(estimate_walk({0,0},{0,181})));
    CHECK(std::holds_alternative<Error>(estimate_walk({0,0},{0,0},0)));
    CHECK(std::holds_alternative<Error>(estimate_walk({0,0},{0,0.1},std::numeric_limits<double>::min())));

    Location loc{{37.5,127},std::nullopt,1000000,1000000,1,1};
    CHECK(classify_location(loc,false,1,1000000)==Freshness::off);
    CHECK(classify_location(std::nullopt,true,1,1000000)==Freshness::no_data);
    CHECK(classify_location(loc,true,2,1000000)==Freshness::no_data);
    CHECK(classify_location(loc,true,1,1029999)==Freshness::fresh);
    CHECK(classify_location(loc,true,1,1030000)==Freshness::delayed);
    CHECK(classify_location(loc,true,1,1119999)==Freshness::delayed);
    CHECK(classify_location(loc,true,1,1120000)==Freshness::stale);
    CHECK(classify_location(loc,true,1,995000)==Freshness::fresh);
    CHECK(classify_location(loc,true,1,994999)==Freshness::no_data);
    std::cout << "PASS: distance, walking estimate and freshness boundaries\n";
}
