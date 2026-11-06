import Foundation
import CoreLocation
import Observation

@Observable final class MeetingPointViewModel {
    enum Method: String, CaseIterable, Identifiable {
        case mean, geometricMedian, minimax
        var id: Self { self }
        var title: String {
            switch self {
            case .mean: "평균 중심"
            case .geometricMedian: "전체 거리 합 최소"
            case .minimax: "가장 먼 거리 최소"
            }
        }
        var detail: String {
            switch self {
            case .mean: "선택한 사람들의 좌표를 평균 내요."
            case .geometricMedian: "모두가 이동할 직선거리의 합을 줄여요."
            case .minimax: "가장 멀리 이동하는 사람의 부담을 줄여요."
            }
        }
        var calculation: MeetPointService.Method {
            switch self {
            case .mean: .mean
            case .geometricMedian: .geometricMedian()
            case .minimax: .minimax
            }
        }
    }

    private(set) var isEnabled = false
    private(set) var coordinate: CLLocationCoordinate2D?
    var selectedIDs: Set<UUID>
    var method: Method = .mean
    var error: String?
    private var knownIDs: Set<UUID>

    init(members: [RoomMember]) {
        knownIDs = Set(members.map(\.id))
        selectedIDs = Set(members.filter { Self.coordinate(for: $0) != nil }.map(\.id))
    }

    static func coordinate(for member: RoomMember) -> CLLocationCoordinate2D? {
        guard member.isSharing, let latitude = member.latitude, let longitude = member.longitude,
              latitude.isFinite, longitude.isFinite else { return nil }
        let coordinate = CLLocationCoordinate2D(latitude: latitude, longitude: longitude)
        return CLLocationCoordinate2DIsValid(coordinate) ? coordinate : nil
    }

    func toggle(_ member: RoomMember) {
        guard Self.coordinate(for: member) != nil else { return }
        if !selectedIDs.insert(member.id).inserted { selectedIDs.remove(member.id) }
        error = nil
    }

    func reconcile(members: [RoomMember]) {
        let eligibleIDs = Set(members.filter { Self.coordinate(for: $0) != nil }.map(\.id))
        let newIDs = Set(members.map(\.id)).subtracting(knownIDs)
        selectedIDs.formIntersection(eligibleIDs)
        selectedIDs.formUnion(newIDs.intersection(eligibleIDs))
        knownIDs = Set(members.map(\.id))
    }

    func setEnabled(_ enabled: Bool, members: [RoomMember]) {
        isEnabled = enabled
        refresh(members: members)
    }

    func refresh(members: [RoomMember]) {
        reconcile(members: members)
        guard isEnabled else { coordinate = nil; error = nil; return }
        coordinate = calculate(members: members)
    }

    func calculate(members: [RoomMember]) -> CLLocationCoordinate2D? {
        let selected = members.filter { selectedIDs.contains($0.id) }
        let coordinates = selected.compactMap { Self.coordinate(for: $0) }
        guard selected.count == selectedIDs.count, coordinates.count == selected.count,
              (1...64).contains(coordinates.count) else {
            error = "위치가 확인된 멤버를 1~64명 선택해 주세요."
            return nil
        }
        do {
            let result = try MeetPointService().calculate(coordinates: coordinates, method: method.calculation)
            guard result.latitude.isFinite, result.longitude.isFinite,
                  CLLocationCoordinate2DIsValid(result) else { throw MeetPointService.CalculationError.invalidInput }
            error = nil
            return result
        } catch { self.error = error.localizedDescription; return nil }
    }
}
