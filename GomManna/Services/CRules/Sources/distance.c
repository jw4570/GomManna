//
//  distance.c
//  GomManna
//
//  Created by 이주원 on 11/2/26.
//

#include "../Include/distance.h"
#include <math.h>

#define EARTH_RADIUS 6371000.0
#define DEG_TO_RAD 0.017453292519943295

/* haversine_distance_m 함수 구현 */

double haversine_distance_m(double lat1, double lon1, double lat2, double lon2) {
    double phi1 = lat1 * DEG_TO_RAD;
    double phi2 = lat2 * DEG_TO_RAD;
    double dphi = (lat2 - lat1) * DEG_TO_RAD;
    double dlambda = (lon2 - lon1) * DEG_TO_RAD;
    
    double s1 = sin(dphi / 2.0);
    double s2 = sin(dlambda / 2.0);
    double a = s1 * s1 + cos(phi1) * cos(phi2) * s2 * s2;
    
    // 부동 소수점 오차로 범위를 벗어나지 않도록 보정
    a = fmax(0.0, fmin(1.0, a));
    
    return 2.0 * EARTH_RADIUS * atan2(sqrt(a), sqrt(1.0 - a));
}
