import Foundation

// 기존 Repository와 Mock이 사용하는 멤버 조회 인터페이스를 유지합니다.
@MainActor
protocol RoomServiceProtocol {
    func fetchMembers(roomID: UUID, token: String) async throws -> [MemberDTO]
}

protocol RoomManagingServiceProtocol: RoomServiceProtocol {
    func createRoom(title: String, nickname: String, token: String) async throws -> RoomMembershipData
    func joinRoom(inviteCode: String, nickname: String, token: String) async throws -> RoomMembershipData
    func fetchRoom(roomID: UUID, token: String) async throws -> RoomDTO
    func fetchInvite(roomID: UUID, token: String) async throws -> InviteDTO
    func leaveRoom(roomID: UUID, token: String) async throws
    func endRoom(roomID: UUID, token: String) async throws
}

struct RoomService: RoomManagingServiceProtocol {
    private let client: APIClient
    init(baseURL: URL, session: URLSession = .shared) { client = APIClient(baseURL: baseURL, session: session) }

    func createRoom(title: String, nickname: String, token: String) async throws -> RoomMembershipData {
        let request = try client.jsonRequest(path: "v1/rooms", method: "POST", token: token,
                                            body: CreateRoomRequest(title: title, nickname: nickname))
        return try await client.send(request, as: RoomMembershipData.self)
    }

    func joinRoom(inviteCode: String, nickname: String, token: String) async throws -> RoomMembershipData {
        let request = try client.jsonRequest(path: "v1/rooms/join", method: "POST", token: token,
                                            body: JoinRoomRequest(inviteCode: inviteCode, nickname: nickname))
        return try await client.send(request, as: RoomMembershipData.self)
    }

    func fetchRoom(roomID: UUID, token: String) async throws -> RoomDTO {
        let data = try await client.send(client.request(path: "v1/rooms/\(roomID)", token: token), as: RoomData.self)
        return data.room
    }

    func fetchMembers(roomID: UUID, token: String) async throws -> [MemberDTO] {
        let data = try await client.send(client.request(path: "v1/rooms/\(roomID)/members", token: token), as: MembersData.self)
        return data.members
    }

    func fetchInvite(roomID: UUID, token: String) async throws -> InviteDTO {
        try await client.send(client.request(path: "v1/rooms/\(roomID)/invite", token: token), as: InviteDTO.self)
    }

    func leaveRoom(roomID: UUID, token: String) async throws {
        let request = try client.jsonRequest(path: "v1/rooms/\(roomID)/leave", method: "POST", token: token, body: EmptyData())
        _ = try await client.send(request, as: EmptyData.self)
    }

    func endRoom(roomID: UUID, token: String) async throws {
        let request = try client.jsonRequest(path: "v1/rooms/\(roomID)/end", method: "POST", token: token, body: EmptyData())
        _ = try await client.send(request, as: EmptyData.self)
    }
}
