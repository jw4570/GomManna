import Foundation

@MainActor
protocol SharingServiceProtocol {
    func updateSharing(roomID: UUID, enabled: Bool, token: String) async throws -> MemberDTO
}

struct SharingService: SharingServiceProtocol {
    private let client: APIClient
    init(baseURL: URL, session: URLSession = .shared) { client = APIClient(baseURL: baseURL, session: session) }

    /// 반환된 sharingEpoch를 이후 위치·메모 요청에 사용합니다.
    func updateSharing(roomID: UUID, enabled: Bool, token: String) async throws -> MemberDTO {
        let request = try client.jsonRequest(path: "v1/rooms/\(roomID)/sharing", method: "PUT", token: token,
                                            body: SharingRequest(enabled: enabled))
        return try await client.send(request, as: MemberData.self).member
    }
}
