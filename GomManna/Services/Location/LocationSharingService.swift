import Foundation
import CoreLocation
import Observation
#if canImport(UIKit)
import UIKit
#endif

/// 화면 수명과 무관하게 세션이 소유합니다. 타이머 대신 Core Location 콜백으로 전송합니다.
@MainActor
@Observable final class LocationSharingService {
    private struct Context: Equatable {
        let roomID: UUID
        let memberID: UUID
        let epoch: Int64
    }
    let device: any LocationProviding
    private let repositories: AppRepositories
    private var context: Context?
    private var generation = UUID()
    private var sequence: Int64 = 0
    private var lastCapturedAt: Int64 = 0
    private var lastAttempt: Date?
    private var pending: CLLocation?
    private var upload: Task<Void, Never>?
    private var suspended = false
    private(set) var enabled = false
    private(set) var message: String?
    private(set) var lastUploadedAt: Date?
    var onServerStateChange: ((Error) async -> Void)?
    private let minimumInterval: TimeInterval

    init(repositories: AppRepositories, device: (any LocationProviding)? = nil, minimumInterval: TimeInterval = 5) {
        self.repositories = repositories
        if let device { self.device = device }
        else {
            #if os(iOS)
            self.device = DeviceLocationService()
            #else
            self.device = InactiveLocationProvider()
            #endif
        }
        self.minimumInterval = minimumInterval
        self.device.onLocation = { [weak self] location in self?.receive(location) }
    }

    func synchronize(_ session: RoomSession?) {
        guard let session, let me = session.room.members.first(where: { $0.id == session.room.myMemberID }), me.isSharing else {
            stop()
            return
        }
        guard !suspended else { return }
        let next = Context(roomID: session.room.id, memberID: me.id, epoch: me.sharingEpoch)
        if context != next {
            resetTransfer()
            context = next
            sequence = me.locationSequence ?? 0
        } else { sequence = max(sequence, me.locationSequence ?? 0) }
        lastCapturedAt = max(lastCapturedAt, me.capturedAtMs ?? 0)
        enabled = true
        device.start()
    }

    /// OFF 요청이나 탈퇴가 진행되는 동안 새 GPS 전송을 먼저 차단합니다.
    func suspend(message: String? = nil) {
        suspended = true
        stop()
        self.message = message
    }
    func allowSharing() { suspended = false; message = nil }
    func stop() {
        resetTransfer()
        device.stop()
        context = nil
        enabled = false
        message = nil
    }

    private func resetTransfer() {
        generation = UUID()
        upload?.cancel()
        upload = nil
        pending = nil
        sequence = 0
        lastCapturedAt = 0
        lastAttempt = nil
        lastUploadedAt = nil
    }

    private func receive(_ location: CLLocation) {
        guard enabled, context != nil, !suspended,
              location.horizontalAccuracy.isFinite, location.horizontalAccuracy >= 0,
              CLLocationCoordinate2DIsValid(location.coordinate),
              abs(location.timestamp.timeIntervalSinceNow) < 60 else { return }
        if let pending, pending.timestamp >= location.timestamp { return }
        pending = location
        sendPending()
    }

    private func sendPending() {
        guard upload == nil, enabled, !suspended, let current = context, let location = pending else { return }
        guard abs(location.timestamp.timeIntervalSinceNow) < 60 else { pending = nil; return }
        let capturedAt = Int64(location.timestamp.timeIntervalSince1970 * 1000)
        guard capturedAt > lastCapturedAt else { pending = nil; return }
        if let lastAttempt, Date().timeIntervalSince(lastAttempt) < minimumInterval { return }
        pending = nil
        lastAttempt = Date()
        sequence += 1
        let nextSequence = sequence
        let operation = generation
        upload = Task { [weak self] in
            guard let self else { return }
            #if os(iOS)
            let background = UIApplication.shared.beginBackgroundTask(withName: "Location upload") { [weak self] in
                Task { @MainActor in
                    if self?.generation == operation { self?.upload?.cancel() }
                }
            }
            defer { if background != .invalid { UIApplication.shared.endBackgroundTask(background) } }
            #endif
            defer {
                if generation == operation {
                    upload = nil
                    sendPending()
                }
            }
            do {
                let token = try await repositories.auth.accessToken()
                try Task.checkCancellation()
                guard generation == operation, enabled, device.isRunning else { return }
                _ = try await repositories.locations.updateLocation(roomID: current.roomID,
                    latitude: location.coordinate.latitude, longitude: location.coordinate.longitude,
                    accuracyMeters: location.horizontalAccuracy, capturedAtMs: capturedAt,
                    sharingEpoch: current.epoch, sequence: nextSequence, token: token)
                try Task.checkCancellation()
                guard generation == operation else { return }
                lastCapturedAt = capturedAt
                lastUploadedAt = Date()
                message = nil
            } catch {
                guard generation == operation, !Task.isCancelled, !error.isRequestCancellation else { return }
                message = "위치를 전송하지 못했어요. 다음 위치 업데이트에서 다시 시도해요."
                if let api = error as? APIRequestError {
                    if [401, 403, 404, 410].contains(api.statusCode) {
                        stop()
                        await onServerStateChange?(error)
                    } else if ["SHARING_OFF", "STALE_UPDATE"].contains(api.code ?? "") {
                        // 회차/sequence를 다시 읽는 동안 GPS 서비스는 유지합니다.
                        await onServerStateChange?(error)
                    }
                }
            }
        }
    }
}
