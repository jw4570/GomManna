import Foundation

struct SharingRepository {
    private let service: any SharingServiceProtocol
    init(service: any SharingServiceProtocol) { self.service = service }

    func updateSharing(roomID: UUID, enabled: Bool, token: String) async throws -> SharingState {
        let member = try await service.updateSharing(roomID: roomID, enabled: enabled, token: token)
        return SharingState(memberID: member.memberID, isSharing: member.isSharing, epoch: member.sharingEpoch)
    }
}
