import Foundation

struct HealthRepository {
    private let service: any HealthServiceProtocol
    init(service: any HealthServiceProtocol) { self.service = service }

    /// DB 상태가 아닌 HTTP 서버의 응답 상태입니다.
    func isHealthy() async throws -> Bool {
        try await service.fetchHealth().status == "ok"
    }
}
