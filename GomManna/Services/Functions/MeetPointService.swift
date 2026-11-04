//
//  MeetPointService.swift
//  GomManna
//
//  Created by 이주원 on 11/4/26.
//

import Foundation
import CoreLocation

struct MeetPointService {
    enum Method {
        /// 참여자 좌표의 평균
        case mean

        /// 전체 이동 거리 합 최소
        case geometricMedian(
            toleranceMeters: Double = 0.1,
            maxIterations: Int = 10_000
        )

        /// 가장 멀리 이동하는 사람의 거리 최소
        case minimax
    }

    enum CalculationError: LocalizedError {
        case invalidInput
        case noConvergence
        case unknown

        var errorDescription: String? {
            switch self {
            case .invalidInput:
                return "좌표 범위, 참여자 수 또는 계산 설정을 확인해 주세요."
            case .noConvergence:
                return "중간 지점을 찾지 못했어요. 계산 설정을 조정해 주세요."
            case .unknown:
                return "중간 지점 계산 중 오류가 발생했어요."
            }
        }
    }

    func calculate(
        coordinates: [CLLocationCoordinate2D],
        method: Method = .mean
    ) throws -> CLLocationCoordinate2D {
        // 현재 C 구현은 1~64명까지 지원합니다.
        guard (1...64).contains(coordinates.count) else {
            throw CalculationError.invalidInput
        }

        let points = coordinates.map {
            GeoPoint(latitude: $0.latitude, longitude: $0.longitude)
        }

        var result = GeoPoint(latitude: 0, longitude: 0)

        let status: MeetingStatus = try points.withUnsafeBufferPointer { buffer in
            switch method {
            case .mean:
                return meeting_mean(
                    buffer.baseAddress,
                    buffer.count,
                    &result
                )

            case let .geometricMedian(tolerance, iterations):
                guard tolerance.isFinite, tolerance > 0, iterations > 0 else {
                    throw CalculationError.invalidInput
                }

                return meeting_geometric_median(
                    buffer.baseAddress,
                    buffer.count,
                    tolerance,
                    iterations,
                    &result
                )

            case .minimax:
                return meeting_minimax(
                    buffer.baseAddress,
                    buffer.count,
                    &result
                )
            }
        }

        switch status {
        case MEETING_OK:
            return CLLocationCoordinate2D(
                latitude: result.latitude,
                longitude: result.longitude
            )
        case MEETING_INVALID_INPUT:
            throw CalculationError.invalidInput
        case MEETING_NO_CONVERGENCE:
            throw CalculationError.noConvergence
        default:
            throw CalculationError.unknown
        }
    }
}
