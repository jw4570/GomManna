#include "gommanna/contracts.hpp"
#include <cstdlib>
#include <iomanip>
#include <iostream>

int main(int argc, char** argv) {
    if(argc!=5) return 2;
    auto result=gommanna::estimate_walk({std::strtod(argv[1],nullptr),std::strtod(argv[2],nullptr)},
                                         {std::strtod(argv[3],nullptr),std::strtod(argv[4],nullptr)});
    if(auto* value=std::get_if<gommanna::TravelEstimate>(&result)) {
        std::cout<<std::setprecision(15)<<value->distance_m<<" "<<value->walk_minutes<<"\n";
        return 0;
    }
    return 1;
}
