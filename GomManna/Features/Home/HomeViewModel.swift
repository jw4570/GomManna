import Foundation
import Observation

@MainActor
@Observable final class HomeViewModel {
    let session: AppSession
    var currentRoom: Room? { session.current?.room }
    var path: [UUID] = []
    var showingCreate = false
    var showingJoin = false
    var showingProfile = false
    var showingSettings = false
    var showingInvite = false
    init(session: AppSession? = nil) { self.session = session ?? .shared }
    func enter(_ room: Room) { showingCreate = false; showingJoin = false; openCurrentRoom() }
    func openCurrentRoom() {
        guard let room = currentRoom else { return }
        path = [room.id]
    }
    func remove(_ room: Room) { showingInvite = false; path.removeAll() }
    func refresh() async {
        do { try await session.refresh() }
        catch {
            guard !Task.isCancelled, !error.isRequestCancellation else { return }
            session.errorMessage = session.describe(error)
        }
    }
    func acceptInvite() {
        guard session.inviteCode != nil else { return }
        if currentRoom == nil { showingJoin = true }
        else { session.errorMessage = "현재 모임에서 나간 뒤 다른 모임에 참여할 수 있어요." }
    }
}
