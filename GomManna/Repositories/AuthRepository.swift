import Foundation

@MainActor
protocol TokenStoring {
    func read() throws -> String?
    func save(_ token: String) throws
}

extension TokenStore: TokenStoring {}

@MainActor
final class AuthRepository {
    private let service: any GuestServiceProtocol
    private let tokenStore: any TokenStoring
    // 잠금 상태에서도 이미 읽은 토큰을 재사용하며 Keychain의 잠금 보호 설정은 유지합니다.
    private var cachedToken: String?
    private var issuance: Task<String, Error>?

    init(service: any GuestServiceProtocol, tokenStore: (any TokenStoring)? = nil) {
        self.service = service
        self.tokenStore = tokenStore ?? TokenStore()
    }

    /// 사용자가 새 익명 사용자 생성을 선택한 경우에만 호출합니다.
    func createNewIdentity() async throws {
        let guest = try await service.createGuest()
        try tokenStore.save(guest.token)
        cachedToken = guest.token
    }

    /// 기존 사용자 토큰을 재사용합니다. Keychain 오류나 401 때문에 새 사용자를 자동 발급하지 않습니다.
    func accessToken() async throws -> String {
        if let cachedToken { return cachedToken }
        if let issuance { return try await issuance.value }
        if let token = try tokenStore.read(), !token.isEmpty {
            cachedToken = token
            return token
        }

        // 같은 Repository에 여러 화면이 요청해도 사용자 발급은 한 번만 수행합니다.
        let task = Task { @MainActor in
            let guest = try await service.createGuest()
            try tokenStore.save(guest.token)
            cachedToken = guest.token
            return guest.token
        }
        issuance = task
        defer { issuance = nil }
        return try await task.value
    }
}
