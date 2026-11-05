import Foundation
import Observation

@MainActor
@Observable final class JoinRoomViewModel {
    enum Method: String, CaseIterable { case code = "코드 입력", qr = "QR 스캔" }
    var method: Method = .code
    var code = ""
    var name = ""
    var error: String?
    private(set) var isLoading = false
    let session: AppSession
    init(session: AppSession? = nil) {
        self.session = session ?? .shared
        code = self.session.inviteCode ?? ""
    }
    var normalizedCode: String { code.trimmingCharacters(in: .whitespacesAndNewlines).uppercased() }
    var canEnter: Bool {
        let allowed = Set("ABCDEFGHJKLMNPQRSTUVWXYZ23456789")
        return !isLoading && normalizedCode.count == 6 && normalizedCode.allSatisfy { allowed.contains($0) }
            && !name.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
    }
    func scanned(_ value: String) {
        guard session.receiveInvite(value) else { method = .code; error = "곰만나 초대 QR을 확인해 주세요."; return }
        code = session.inviteCode ?? ""
        method = .code
        error = nil
    }
    func join() async -> Room? {
        guard canEnter else { return nil }
        isLoading = true
        defer { isLoading = false }
        do {
            let token = try await session.repositories.auth.accessToken()
            let result = try await session.repositories.rooms.joinRoom(inviteCode: normalizedCode,
                nickname: name.trimmingCharacters(in: .whitespacesAndNewlines), token: token)
            session.enter(result)
            session.inviteCode = nil
            error = nil
            return result.room
        } catch { self.error = session.describe(error); return nil }
    }
}
