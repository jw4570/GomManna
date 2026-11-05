import Foundation
import Observation

@MainActor
@Observable final class AppSession {
    static let shared = AppSession()
    let repositories: AppRepositories
    private let defaults: UserDefaults
    private let storageKey: String
    let locationSharing: LocationSharingService
    private(set) var current: RoomSession? {
        didSet { locationSharing.synchronize(current) }
    }
    private(set) var ready = false
    private(set) var preparing = false
    var errorMessage: String?
    var inviteCode: String?
    private(set) var recoveryNeeded = false
    private var refreshing = false
    private var stateRevision = 0
    private struct Membership: Codable { let roomID: UUID; let userID: UUID }

    init(repositories: AppRepositories? = nil, defaults: UserDefaults = .standard, storageKey: String? = nil, locationProvider: (any LocationProviding)? = nil, minimumLocationInterval: TimeInterval = 5) {
        // Configuration uses a valid default HTTPS URL and does not require manual access tokens.
        let resolved = repositories ?? AppEnvironment.makeRepositories()
        self.repositories = resolved
        self.locationSharing = LocationSharingService(repositories: resolved, device: locationProvider, minimumInterval: minimumLocationInterval)
        self.defaults = defaults
        self.storageKey = storageKey ?? AppEnvironment.persistenceKey
        locationSharing.onServerStateChange = { [weak self] error in
            await self?.reconcileLocationError(error)
        }
    }

    static func preview() -> AppSession {
        let value = AppSession(repositories: .mock(service: MockAPIService()), storageKey: "GomManna.preview")
        return value
    }

    func prepare() async {
        guard !preparing, !ready else { return }
        preparing = true
        defer { preparing = false }
        do {
            _ = try await repositories.auth.accessToken()
            if let data = defaults.data(forKey: storageKey),
               let saved = try? JSONDecoder().decode(Membership.self, from: data) {
                do {
                    let token = try await repositories.auth.accessToken()
                    current = try await repositories.rooms.fetchRoom(roomID: saved.roomID, currentUserID: saved.userID, token: token)
                } catch {
                    if isMissingRoom(error) { clearRoom() } else { throw error }
                }
            }
            ready = true
            errorMessage = nil
        } catch {
            guard !Task.isCancelled, !error.isRequestCancellation else { return }
            errorMessage = describe(error)
        }
    }

    func describe(_ error: Error) -> String {
        if let api = error as? APIRequestError, api.statusCode == 401 {
            recoveryNeeded = true
            locationSharing.suspend()
            return "인증이 만료되었어요. 새 익명 사용자로 시작하면 기존 모임에는 초대 코드로 다시 참여해야 해요."
        }
        return error.localizedDescription
    }

    func recoverIdentity() async {
        guard !preparing else { return }
        preparing = true
        defer { preparing = false }
        do {
            try await repositories.auth.createNewIdentity()
            clearRoom()
            recoveryNeeded = false
            ready = true
            errorMessage = nil
        } catch {
            guard !Task.isCancelled, !error.isRequestCancellation else { return }
            errorMessage = describe(error)
        }
    }

    func enter(_ session: RoomSession) {
        stateRevision += 1
        locationSharing.allowSharing()
        current = session
        let saved = Membership(roomID: session.room.id, userID: session.myUserID)
        defaults.set(try? JSONEncoder().encode(saved), forKey: storageKey)
    }

    func refresh() async throws {
        try Task.checkCancellation()
        while refreshing { try await Task.sleep(for: .milliseconds(50)) }
        guard let snapshot = current else { return }
        let revision = stateRevision
        refreshing = true
        defer { refreshing = false }
        do {
            let token = try await repositories.auth.accessToken()
            let updated = try await repositories.rooms.fetchRoom(roomID: snapshot.room.id, currentUserID: snapshot.myUserID, token: token)
            try Task.checkCancellation()
            guard current?.room.id == snapshot.room.id, stateRevision == revision else { return }
            current = updated
        } catch {
            guard current?.room.id == snapshot.room.id, stateRevision == revision else { return }
            if isMissingRoom(error) {
                clearRoom()
                errorMessage = "모임이 종료되었거나 더 이상 참여 중이 아니에요."
            }
            throw error
        }
    }

    func clearRoom() {
        stateRevision += 1
        current = nil
        locationSharing.allowSharing()
        defaults.removeObject(forKey: storageKey)
    }

    func resumeLocationSharingIfNeeded() { locationSharing.synchronize(current) }

    @discardableResult
    func setSharing(_ enabled: Bool, roomID: UUID) async throws -> SharingState {
        guard let snapshot = current, snapshot.room.id == roomID else { throw CancellationError() }
        stateRevision += 1
        if !enabled { locationSharing.suspend() }
        let token = try await repositories.auth.accessToken()
        let state = try await repositories.sharing.updateSharing(roomID: roomID, enabled: enabled, token: token)
        guard let latest = current, latest.room.id == roomID else { throw CancellationError() }
        stateRevision += 1
        var room = latest.room
        room.members = room.members.map { member in
            guard member.id == state.memberID else { return member }
            var updated = member
            if !state.isSharing || member.sharingEpoch != state.epoch {
                updated.latitude = nil; updated.longitude = nil; updated.note = nil
                updated.capturedAtMs = nil; updated.locationSequence = nil
            }
            updated.isSharing = state.isSharing
            updated.sharingEpoch = state.epoch
            updated.locationStatus = state.isSharing ? (updated.latitude == nil ? .noData : member.locationStatus) : .off
            return updated
        }
        if enabled { locationSharing.allowSharing() }
        current = RoomSession(room: room, hostUserID: latest.hostUserID, myUserID: latest.myUserID,
            sharingEpoch: state.epoch, status: latest.status, createdAtMs: latest.createdAtMs, endedAtMs: latest.endedAtMs)
        return state
    }

    private func reconcileLocationError(_ error: Error) async {
        if let api = error as? APIRequestError, api.statusCode == 401 {
            errorMessage = describe(error)
        } else if isMissingRoom(error) {
            clearRoom()
            errorMessage = "모임이 종료되었거나 더 이상 참여 중이 아니에요."
        } else {
            do { try await refresh() }
            catch {
                if !Task.isCancelled && !error.isRequestCancellation { errorMessage = describe(error) }
            }
        }
    }

    func receiveInvite(_ text: String) -> Bool {
        let raw = text.trimmingCharacters(in: .whitespacesAndNewlines)
        let code: String
        if let parts = URLComponents(string: raw), parts.scheme?.lowercased() == "gommanna", parts.host == "join" {
            code = parts.queryItems?.first(where: { $0.name == "code" })?.value ?? ""
        } else { code = raw }
        let normalized = code.uppercased()
        let alphabet = Set("ABCDEFGHJKLMNPQRSTUVWXYZ23456789")
        guard normalized.count == 6, normalized.allSatisfy({ alphabet.contains($0) }) else { return false }
        inviteCode = normalized
        return true
    }

    private func isMissingRoom(_ error: Error) -> Bool {
        if let api = error as? APIRequestError { return [403, 404, 410].contains(api.statusCode) }
        if let repo = error as? RoomRepository.RepositoryError,
           case .currentMemberNotFound = repo { return true }
        return false
    }
}
