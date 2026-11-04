import Foundation

/// 실제 서비스와 같은 DTO를 반환해 Repository의 변환 과정까지 확인합니다.
struct MockRoomService: RoomServiceProtocol {
    let room: Room

    func fetchMembers(roomID: UUID, token: String) async throws -> [MemberDTO] {
        try Task.checkCancellation()
        guard roomID == room.id else {
            throw APIRequestError(statusCode: 404, message: "Mock 모임을 찾을 수 없어요.")
        }
        let now = Int64(Date().timeIntervalSince1970 * 1000)
        return room.members.map { member in
            let location: LocationDTO?
            if member.isSharing, let latitude = member.latitude, let longitude = member.longitude {
                location = LocationDTO(latitude: latitude, longitude: longitude, accuracyM: 10,
                                       capturedAtMs: now, receivedAtMs: now, sharingEpoch: 1, sequence: 1)
            } else {
                location = nil
            }
            return MemberDTO(memberID: member.id, roomID: roomID, userID: member.userID,
                             nickname: member.name, isSharing: member.isSharing, sharingEpoch: 1,
                             joinedAtMs: now, locationStatus: member.isSharing ? (location == nil ? .noData : .fresh) : .off,
                             location: location, note: nil)
        }
    }
}
