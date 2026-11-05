import Foundation
import Observation

@MainActor
@Observable final class CreateRoomViewModel {
    var title = ""
    var name = ""
    var error: String?
    private(set) var isLoading = false
    let session: AppSession
    init(session: AppSession? = nil) { self.session = session ?? .shared }
    var canCreate: Bool {
        !isLoading && !title.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty && !name.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
    }
    func create() async -> Room? {
        guard canCreate else { return nil }
        isLoading = true
        defer { isLoading = false }
        do {
            let token = try await session.repositories.auth.accessToken()
            let result = try await session.repositories.rooms.createRoom(
                title: title.trimmingCharacters(in: .whitespacesAndNewlines),
                nickname: name.trimmingCharacters(in: .whitespacesAndNewlines), token: token)
            session.enter(result)
            error = nil
            return result.room
        } catch { self.error = session.describe(error); return nil }
    }
}
