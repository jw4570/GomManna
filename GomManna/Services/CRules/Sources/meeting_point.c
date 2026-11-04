//
//  meeting_point.h
//  GomManna
//
//  Created by 이주원 on 11/2/26.
//

#include "../Include/meeting_point.h"
#include <float.h>
#include <math.h>

#define MAX_PEOPLE 64
#define EARTH_RADIUS 6371000.0
#define DEG_TO_RAD 0.017453292519943295
#define PI 3.141592653589793238

/* 미터 단위 평면 좌표 */
typedef struct {
    double x;
    double y;
} XY;

/* 위도·경도 변환 기준 */
typedef struct {
    double lat0;
    double lon0;
    double scale;
} Projection;

static double distance(XY a, XY b) {
    return hypot(a.x - b.x, a.y - b.y);
}

/* 위도·경도 → 미터 좌표 */
static int project(
    const GeoPoint *points,
    size_t count,
    XY *xy,
    Projection *p,
    GeoPoint *result
) {
    if (!points || !result || count == 0 || count > MAX_PEOPLE)
        return 0;

    for (size_t i = 0; i < count; ++i) {
        if (!isfinite(points[i].latitude) ||
            !isfinite(points[i].longitude) ||
            fabs(points[i].latitude) > 85.0 ||
            fabs(points[i].longitude) > 180.0)
            return 0;
    }

    p->lat0 = points[0].latitude * DEG_TO_RAD;
    p->lon0 = points[0].longitude * DEG_TO_RAD;
    p->scale = cos(p->lat0);

    for (size_t i = 0; i < count; ++i) {
        double dlon = remainder(
            points[i].longitude * DEG_TO_RAD - p->lon0,
            2 * PI
        );

        xy[i].x = EARTH_RADIUS * p->scale * dlon;
        xy[i].y = EARTH_RADIUS *
            (points[i].latitude * DEG_TO_RAD - p->lat0);

        if (hypot(xy[i].x, xy[i].y) > 200000.0)
            return 0;
    }

    return 1;
}

/* 미터 좌표 → 위도·경도 */
static GeoPoint unproject(XY xy, Projection p) {
    GeoPoint result;

    result.latitude =
        (p.lat0 + xy.y / EARTH_RADIUS) / DEG_TO_RAD;

    result.longitude = remainder(
        p.lon0 + xy.x / (EARTH_RADIUS * p.scale),
        2 * PI
    ) / DEG_TO_RAD;

    return result;
}

static XY mean_xy(const XY *points, size_t count) {
    XY result = {0, 0};

    for (size_t i = 0; i < count; ++i) {
        result.x += points[i].x;
        result.y += points[i].y;
    }

    result.x /= count;
    result.y /= count;

    return result;
}

/*
 * 수정 Weiszfeld 알고리즘.
 * 현재 계산 지점과 참여자 좌표가 겹치는 경우도 처리합니다.
 */
static MeetingStatus median_xy(
    const XY *points,
    size_t count,
    double tolerance,
    size_t iterations,
    XY *result
) {
    XY current = mean_xy(points, count);

    for (size_t step = 0; step < iterations; ++step) {
        XY weighted = {0, 0};
        XY direction = {0, 0};
        double total_weight = 0;
        size_t coincident = 0;

        for (size_t i = 0; i < count; ++i) {
            double d = distance(current, points[i]);

            if (d <= 1e-9) {
                ++coincident;
                continue;
            }

            double weight = 1.0 / d;

            weighted.x += points[i].x * weight;
            weighted.y += points[i].y * weight;

            direction.x +=
                (points[i].x - current.x) * weight;
            direction.y +=
                (points[i].y - current.y) * weight;

            total_weight += weight;
        }

        double norm = hypot(direction.x, direction.y);

        if (total_weight == 0 ||
            (coincident > 0 && norm <= (double)coincident)) {
            *result = current;
            return MEETING_OK;
        }

        double keep = coincident > 0
            ? fmin(1.0, (double)coincident / norm)
            : 0;

        XY next = {
            (1 - keep) * weighted.x / total_weight
                + keep * current.x,
            (1 - keep) * weighted.y / total_weight
                + keep * current.y
        };

        if (distance(current, next) <= tolerance) {
            *result = next;
            return MEETING_OK;
        }

        current = next;
    }

    return MEETING_NO_CONVERGENCE;
}

/* 1. 전체 참여자의 평균 중심 */
MeetingStatus meeting_mean(
    const GeoPoint *points,
    size_t count,
    GeoPoint *result
) {
    XY xy[MAX_PEOPLE];
    Projection p;

    if (!project(points, count, xy, &p, result))
        return MEETING_INVALID_INPUT;

    *result = unproject(mean_xy(xy, count), p);
    return MEETING_OK;
}

/* 2. 전체 참여자까지의 거리 합 최소 */
MeetingStatus meeting_geometric_median(
    const GeoPoint *points,
    size_t count,
    double tolerance_m,
    size_t max_iterations,
    GeoPoint *result
) {
    XY xy[MAX_PEOPLE];
    XY center;
    Projection p;

    if (!isfinite(tolerance_m) ||
        tolerance_m <= 0 ||
        max_iterations == 0 ||
        !project(points, count, xy, &p, result))
        return MEETING_INVALID_INPUT;

    MeetingStatus status = median_xy(
        xy, count, tolerance_m, max_iterations, &center
    );

    if (status == MEETING_OK)
        *result = unproject(center, p);

    return status;
}

/* 후보 지점에서 가장 먼 사람까지의 거리 비교 */
static void consider(
    XY center,
    const XY *points,
    size_t count,
    XY *best,
    double *best_radius
) {
    if (!isfinite(center.x) || !isfinite(center.y))
        return;

    double radius = 0;

    for (size_t i = 0; i < count; ++i)
        radius = fmax(radius, distance(center, points[i]));

    if (radius < *best_radius) {
        *best_radius = radius;
        *best = center;
    }
}

/*
 * 4. 가장 멀리 이동하는 사람의 거리 최소
 *
 * 평면 최소 포함원의 중심을 찾습니다.
 * 1점, 2점 중점, 3점 외심을 후보로 비교합니다.
 */
MeetingStatus meeting_minimax(
    const GeoPoint *points,
    size_t count,
    GeoPoint *result
) {
    XY xy[MAX_PEOPLE];
    XY best = {0, 0};
    Projection p;

    if (!project(points, count, xy, &p, result))
        return MEETING_INVALID_INPUT;

    double best_radius = DBL_MAX;

    for (size_t i = 0; i < count; ++i) {
        consider(xy[i], xy, count, &best, &best_radius);

        for (size_t j = i + 1; j < count; ++j) {
            XY midpoint = {
                (xy[i].x + xy[j].x) / 2,
                (xy[i].y + xy[j].y) / 2
            };

            consider(
                midpoint, xy, count, &best, &best_radius
            );

            for (size_t k = j + 1; k < count; ++k) {
                double bx = xy[j].x - xy[i].x;
                double by = xy[j].y - xy[i].y;
                double cx = xy[k].x - xy[i].x;
                double cy = xy[k].y - xy[i].y;

                double b2 = bx * bx + by * by;
                double c2 = cx * cx + cy * cy;
                double determinant = 2 * (bx * cy - by * cx);

                /* 세 점이 일직선이면 외심 계산 생략 */
                if (fabs(determinant) <=
                    1e-12 * fmax(1.0, fmax(b2, c2)))
                    continue;

                XY center = {
                    xy[i].x +
                        (cy * b2 - by * c2) / determinant,
                    xy[i].y +
                        (bx * c2 - cx * b2) / determinant
                };

                consider(
                    center, xy, count, &best, &best_radius
                );
            }
        }
    }

    *result = unproject(best, p);
    return MEETING_OK;
}
