import Foundation
import CoreLocation

@MainActor private final class ManualLocationProvider: LocationProviding {
    var onLocation: ((CLLocation) -> Void)?
    var message: String?
    var isRunning = false
    func start() { isRunning = true }
    func stop() { isRunning = false }
    func emit(_ offset: Double = 0) {
        guard isRunning else { return }
        onLocation?(CLLocation(coordinate: CLLocationCoordinate2D(latitude: 37.5665 + offset, longitude: 126.978),
            altitude: 0, horizontalAccuracy: 5, verticalAccuracy: 5,
            timestamp: Date(timeIntervalSinceNow: 0.1 + offset)))
    }
}

@MainActor private final class RecordingLocationAPI: LocationServiceProtocol {
    let backend: MockAPIService
    var calls: [LocationRequest] = []
    var holdNext = false
    var release: CheckedContinuation<Void, Never>?
    var finished = 0
    init(backend: MockAPIService) { self.backend = backend }
    func updateLocation(roomID: UUID, location: LocationRequest, token: String) async throws -> LocationDTO {
        calls.append(location)
        defer { finished += 1 }
        if holdNext {
            holdNext = false
            await withCheckedContinuation { release = $0 }
            release = nil
            try Task.checkCancellation()
        }
        return try await backend.updateLocation(roomID: roomID, location: location, token: token)
    }
}

@MainActor private final class LockingTokenStore: TokenStoring {
    let token: String
    var locked = false
    var reads = 0
    init(token: String) { self.token = token }
    func read() throws -> String? {
        reads += 1
        if locked { throw URLError(.userAuthenticationRequired) }
        return token
    }
    func save(_ token: String) throws {}
}

enum BackgroundLocationChecks {
    @MainActor private static func until(_ condition: () -> Bool) async throws {
        let limit = Date().addingTimeInterval(5)
        while !condition() {
            guard Date() < limit else { throw URLError(.timedOut) }
            try await Task.sleep(for: .milliseconds(2))
        }
    }

    @MainActor static func run() async throws {
        let backend = MockAPIService()
        let recording = RecordingLocationAPI(backend: backend)
        let provider = ManualLocationProvider()
        let tokens = LockingTokenStore(token: backend.defaultToken)
        let repos = AppRepositories(auth: AuthRepository(service: backend, tokenStore: tokens),
            rooms: RoomRepository(service: backend), sharing: SharingRepository(service: backend),
            locations: LocationRepository(service: recording), notes: NoteRepository(service: backend),
            photos: PhotoRepository(service: backend), health: HealthRepository(service: backend))
        let suite = "GomManna.background-test.\(UUID())"
        let defaults = UserDefaults(suiteName: suite)!
        defer { defaults.removePersistentDomain(forName: suite) }
        let session = AppSession(repositories: repos, defaults: defaults, storageKey: "room",
                                 locationProvider: provider, minimumLocationInterval: 0)
        let sample = MockRoomData.room
        let initial = try await repos.rooms.fetchRoom(roomID: sample.id, currentUserID: sample.members[0].userID, token: backend.defaultToken)
        session.enter(initial)
        precondition(provider.isRunning && session.locationSharing.enabled)

        // RoomView/RoomViewModel 없이 세션의 GPS 콜백만으로 전송합니다.
        provider.emit()
        try await until { recording.finished == 1 }
        precondition(recording.calls[0].sequence == 2 && session.locationSharing.lastUploadedAt != nil)

        tokens.locked = true // 화면 잠금 후 Keychain 접근이 불가능해도 메모리 토큰으로 전송합니다.
        recording.holdNext = true
        provider.emit(0.001)
        try await until { recording.release != nil }
        provider.emit(0.002)
        provider.emit(0.003)
        precondition(recording.calls.count == 2) // 동시에 여러 요청을 보내지 않습니다.
        recording.release?.resume()
        try await until { recording.finished == 3 }
        precondition(recording.calls[2].latitude == 37.5665 + 0.003)
        precondition(recording.calls[2].sequence > recording.calls[1].sequence)

        recording.holdNext = true
        provider.emit(0.004)
        try await until { recording.release != nil }
        try await session.setSharing(false, roomID: sample.id)
        precondition(!provider.isRunning && !session.locationSharing.enabled)
        recording.release?.resume()
        try await until { recording.finished == 4 }
        precondition(session.locationSharing.lastUploadedAt == nil)
        provider.emit(0.005)
        precondition(recording.calls.count == 4)

        let next = try await session.setSharing(true, roomID: sample.id)
        precondition(next.epoch > initial.sharingEpoch && provider.isRunning)
        provider.emit(0.006)
        try await until { recording.finished == 5 }
        precondition(recording.calls[4].sharingEpoch == next.epoch && recording.calls[4].sequence == 1)

        // 다른 기기에서 OFF/ON: 오래된 epoch 응답을 받으면 재조회하고 GPS를 유지합니다.
        _ = try await backend.updateSharing(roomID: sample.id, enabled: false, token: backend.defaultToken)
        let remote = try await backend.updateSharing(roomID: sample.id, enabled: true, token: backend.defaultToken)
        provider.emit(0.007)
        try await until { session.current?.sharingEpoch == remote.sharingEpoch }
        precondition(provider.isRunning)
        provider.emit(0.008)
        try await until { recording.finished == 7 }
        precondition(recording.calls[6].sharingEpoch == remote.sharingEpoch && recording.calls[6].sequence == 1)

        // 외부 종료를 다음 전송 응답으로 감지하면 GPS와 세션을 함께 정리합니다.
        try await backend.endRoom(roomID: sample.id, token: backend.defaultToken)
        provider.emit(0.009)
        try await until { session.current == nil }
        precondition(!provider.isRunning && !session.locationSharing.enabled)
        precondition(tokens.reads == 1)
        print("PASS: session-owned GPS upload, single request/latest pending, OFF cancels, epoch/sequence recovery, remote end stops GPS")
    }
}
