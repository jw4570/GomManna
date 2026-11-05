import Foundation

@MainActor
protocol HealthServiceProtocol {
    func fetchHealth() async throws -> HealthDTO
}

struct HealthService: HealthServiceProtocol {
    private let client: APIClient
    init(baseURL: URL, session: URLSession = .shared) { client = APIClient(baseURL: baseURL, session: session) }

    /// 서버 프로세스 확인용이며 DB 연결 성공을 보장하지 않습니다.
    func fetchHealth() async throws -> HealthDTO {
        try await client.send(client.request(path: "health"), as: HealthDTO.self)
    }
}
