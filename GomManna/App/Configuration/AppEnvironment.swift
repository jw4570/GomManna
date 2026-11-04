import Foundation

enum AppEnvironment {
    enum DataSource { case mock, live }

    // 여기만 .live로 바꾸면 실제 API를 사용합니다.
    static let dataSource: DataSource = .mock

    static var initialRoom: Room? {
        switch dataSource {
        case .mock: MockRoomData.room
        case .live: nil
        }
    }

    /// Live 모드는 서버에서 받은 방/멤버 ID와 인증 토큰이 필요합니다.
    static func memberDependencies(for room: Room) throws -> (repository: RoomRepository, hostUserID: UUID, token: String) {
        guard let host = room.members.first(where: \.isHost) else {
            throw ConfigurationError(message: "방장 정보를 먼저 불러와 주세요.")
        }
        switch dataSource {
        case .mock:
            return (RoomRepository(service: MockRoomService(room: room), initialMembers: room.members), host.userID, "mock-token")
        case .live:
            let environment = ProcessInfo.processInfo.environment
            guard let address = environment["GOMMANNA_API_BASE_URL"],
                  let url = URL(string: address),
                  ["http", "https"].contains(url.scheme?.lowercased() ?? ""),
                  url.host != nil,
                  let token = environment["GOMMANNA_ACCESS_TOKEN"], !token.isEmpty else {
                throw ConfigurationError(message: "API 주소와 인증 토큰을 설정해 주세요.")
            }
            return (RoomRepository(service: RoomService(baseURL: url), initialMembers: room.members), host.userID, token)
        }
    }

    private struct ConfigurationError: LocalizedError {
        let message: String
        var errorDescription: String? { message }
    }
}
