import Foundation

struct LocationRepository {
    private let service: any LocationServiceProtocol
    init(service: any LocationServiceProtocol) { self.service = service }

    /// epoch는 공유 ON 응답에서 받습니다. sequence는 같은 epoch에서 증가시켜 전달합니다.
    func updateLocation(roomID: UUID, latitude: Double, longitude: Double,
                        accuracyMeters: Double?, capturedAtMs: Int64,
                        sharingEpoch: Int64, sequence: Int64, token: String) async throws -> MemberLocation {
        let request = LocationRequest(latitude: latitude, longitude: longitude, accuracyM: accuracyMeters,
                                      capturedAtMs: capturedAtMs, sharingEpoch: sharingEpoch, sequence: sequence)
        let location = try await service.updateLocation(roomID: roomID, location: request, token: token)
        return MemberLocation(latitude: location.latitude, longitude: location.longitude,
                              accuracyMeters: location.accuracyM, capturedAtMs: location.capturedAtMs,
                              receivedAtMs: location.receivedAtMs, sharingEpoch: location.sharingEpoch,
                              sequence: location.sequence)
    }
}
