import Foundation

@MainActor
final class RoomRepository {
    private let service: any RoomServiceProtocol
    
    private var characters: [UUID: AnimalCharacter] = [:]
    
    init(service: any RoomServiceProtocol, initialMembers: [RoomMember] = []) {
        self.service = service
        for member in initialMembers {
            characters[member.userID] = member.character
        }
    }
    
    func fetchMembers(roomID: UUID, hostUserID: UUID, token: String) async throws -> [RoomMember] {
        let dtos = try await service.fetchMembers(roomID: roomID, token: token)
        
        return dtos.map { member(from: $0, hostUserID: hostUserID) }
    }

    func createRoom(title: String, nickname: String, token: String) async throws -> RoomSession {
        let response = try await managementService().createRoom(title: title, nickname: nickname, token: token)
        return session(from: response)
    }

    func joinRoom(inviteCode: String, nickname: String, token: String) async throws -> RoomSession {
        let response = try await managementService().joinRoom(inviteCode: inviteCode, nickname: nickname, token: token)
        return session(from: response)
    }

    func fetchRoom(roomID: UUID, currentUserID: UUID, token: String) async throws -> RoomSession {
        let room = try await managementService().fetchRoom(roomID: roomID, token: token)
        let dtos = try await service.fetchMembers(roomID: roomID, token: token)
        let members = dtos.map { member(from: $0, hostUserID: room.hostID) }
        guard let me = members.first(where: { $0.userID == currentUserID }) else {
            throw RepositoryError.currentMemberNotFound
        }
        guard let dto = dtos.first(where: { $0.userID == currentUserID }) else {
            throw RepositoryError.currentMemberNotFound
        }
        return makeSession(room: room, me: me, members: members, epoch: dto.sharingEpoch)
    }

    func fetchInvite(roomID: UUID, token: String) async throws -> RoomInvitation {
        let invite = try await managementService().fetchInvite(roomID: roomID, token: token)
        return RoomInvitation(code: invite.inviteCode, qrPayload: invite.qrPayload, qrSVG: invite.qrSVG)
    }

    func leaveRoom(roomID: UUID, token: String) async throws {
        try await managementService().leaveRoom(roomID: roomID, token: token)
    }

    func endRoom(roomID: UUID, token: String) async throws {
        try await managementService().endRoom(roomID: roomID, token: token)
    }

    private func managementService() throws -> any RoomManagingServiceProtocol {
        guard let service = service as? any RoomManagingServiceProtocol else {
            throw RepositoryError.unsupportedOperation
        }
        return service
    }

    private func session(from response: RoomMembershipData) -> RoomSession {
        let me = member(from: response.member, hostUserID: response.room.hostID)
        // 생성·참가 API는 내 멤버만 반환합니다. 전체 목록은 fetchRoom/fetchMembers로 갱신합니다.
        return makeSession(room: response.room, me: me, members: [me], epoch: response.member.sharingEpoch)
    }

    private func makeSession(room: RoomDTO, me: RoomMember, members: [RoomMember], epoch: Int64) -> RoomSession {
        RoomSession(room: Room(id: room.roomID, title: room.title, code: room.inviteCode,
                               myName: me.name, isHost: me.isHost, members: members, myMemberID: me.id),
                    hostUserID: room.hostID, myUserID: me.userID, sharingEpoch: epoch,
                    status: room.status, createdAtMs: room.createdAtMs, endedAtMs: room.endedAtMs)
    }

    private func member(from dto: MemberDTO, hostUserID: UUID) -> RoomMember {
        let character = characters[dto.userID] ?? .random()
        characters[dto.userID] = character
        let location = dto.isSharing ? dto.location : nil
        return RoomMember(id: dto.memberID, userID: dto.userID, name: dto.nickname,
                          character: character, isHost: dto.userID == hostUserID,
                          isSharing: dto.isSharing, latitude: location?.latitude, longitude: location?.longitude,
                          sharingEpoch: dto.sharingEpoch, locationStatus: dto.locationStatus,
                          capturedAtMs: location?.capturedAtMs, locationSequence: location?.sequence,
                          note: dto.isSharing ? dto.note.map { MemberNote(dto: $0) } : nil)
    }

    enum RepositoryError: LocalizedError {
        case unsupportedOperation
        case currentMemberNotFound

        var errorDescription: String? {
            switch self {
            case .unsupportedOperation: "이 데이터 소스는 방 관리 기능을 지원하지 않아요."
            case .currentMemberNotFound: "현재 사용자의 방 참가 정보를 찾을 수 없어요."
            }
        }
    }
}
