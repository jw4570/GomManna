import Foundation
import Observation

@MainActor
@Observable final class RoomSettingsViewModel {
    var confirmingRemoval = false
    var error: String?
    private(set) var isLoading = false
    func remove(room: Room, session: AppSession) async -> Bool {
        guard !isLoading else { return false }
        isLoading = true
        session.locationSharing.suspend()
        defer { isLoading = false }
        do {
            let token = try await session.repositories.auth.accessToken()
            if room.isHost { try await session.repositories.rooms.endRoom(roomID: room.id, token: token) }
            else { try await session.repositories.rooms.leaveRoom(roomID: room.id, token: token) }
            session.clearRoom()
            return true
        } catch {
            session.locationSharing.suspend(message: "위치 전송을 일시 중지했어요. 공유를 껐다 켜면 재개돼요.")
            self.error = session.describe(error)
            return false
        }
    }
}
