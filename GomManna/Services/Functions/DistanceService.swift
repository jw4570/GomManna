//
//  MemberDistance.swift
//  GomManna
//
//  Created by 이주원 on 11/4/26.
//

import Foundation
import CoreLocation

struct DistanceService {
    enum CalculationError: LocalizedError {
        case invalidCoordinate
        case invalidResult

        var errorDescription: String? {
            switch self {
            case .invalidCoordinate:
                return "위도와 경도를 확인해 주세요."
            case .invalidResult:
                return "거리를 계산하지 못했어요."
            }
        }
    }

    func calculate(
        from start: CLLocationCoordinate2D,
        to end: CLLocationCoordinate2D
    ) throws -> Double {
        guard isValid(start), isValid(end) else {
            throw CalculationError.invalidCoordinate
        }

        let meters = haversine_distance_m(
            start.latitude,
            start.longitude,
            end.latitude,
            end.longitude
        )

        guard meters.isFinite, meters >= 0 else {
            throw CalculationError.invalidResult
        }

        return meters
    }

    private func isValid(_ coordinate: CLLocationCoordinate2D) -> Bool {
        coordinate.latitude.isFinite &&
        coordinate.longitude.isFinite &&
        CLLocationCoordinate2DIsValid(coordinate)
    }
}
