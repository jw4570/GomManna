//
//  meeting_point.h
//  GomManna
//
//  Created by 이주원 on 11/2/26.
//

#ifndef MEETING_POINT_H
#define MEETING_POINT_H

#include <stddef.h>

/* 좌표: 도(degree) 단위 */
typedef struct {
    double latitude;
    double longitude;
} GeoPoint;

/* 함수 실행 결과 */
typedef enum {
    MEETING_OK = 0,
    MEETING_INVALID_INPUT = 1,
    MEETING_NO_CONVERGENCE = 2
} MeetingStatus;

/*
 * 공통 규칙
 * - points: 참여자 좌표 배열
 * - count: 참여자 수, 1명 이상
 * - result: 계산한 좌표를 저장할 공간
 * - 성공한 경우에만 result를 사용합니다.
 * - 동일 도시 등 좁은 범위의 좌표를 대상으로 구현합니다.
 */

/* 1. 전체 참여자의 평균 중심 */
MeetingStatus meeting_mean(
    const GeoPoint *points,
    size_t count,
    GeoPoint *result
);

/*
 * 2. 전체 참여자까지의 거리 합이 최소인 지점
 * tolerance_m: 수렴 판단 거리, 미터 단위
 * max_iterations: 최대 반복 횟수
 */
MeetingStatus meeting_geometric_median(
    const GeoPoint *points,
    size_t count,
    double tolerance_m,
    size_t max_iterations,
    GeoPoint *result
);

/*
 * 3. 가까이 모인 과반수 그룹의 중심
 * cluster_radius_m: 그룹 판단 반경, 미터 단위
 * 과반수 그룹이 없으면 거리 합 최소 방식으로 계산합니다.
 */
MeetingStatus meeting_cluster_center(
    const GeoPoint *points,
    size_t count,
    double cluster_radius_m,
    GeoPoint *result
);

/* 4. 가장 멀리 이동하는 사람의 거리가 최소인 지점 */
MeetingStatus meeting_minimax(
    const GeoPoint *points,
    size_t count,
    GeoPoint *result
);

#endif /* MEETING_POINT_H */
