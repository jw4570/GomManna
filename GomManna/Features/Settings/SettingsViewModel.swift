import Foundation
import Observation
import CoreLocation
import AVFoundation

@MainActor
@Observable final class SettingsViewModel {
    let session: AppSession
    var locationPermission = "확인 중"
    var cameraPermission = "확인 중"
    var error: String?
    var busy = false
    init(session: AppSession) { self.session = session }
    var room: Room? { session.current?.room }
    var me: RoomMember? { room?.members.first { $0.id == room?.myMemberID } }
    var version: String {
        let info = Bundle.main.infoDictionary ?? [:]
        return "\(info["CFBundleShortVersionString"] as? String ?? "1.0") (\(info["CFBundleVersion"] as? String ?? "1"))"
    }
    func readPermissions() {
        switch CLLocationManager().authorizationStatus {
        case .authorizedAlways: locationPermission = "항상 허용"
        case .authorizedWhenInUse: locationPermission = "앱 사용 중 허용"
        case .notDetermined: locationPermission = "선택 전"
        default: locationPermission = "허용 안 함"
        }
        switch AVCaptureDevice.authorizationStatus(for: .video) {
        case .authorized: cameraPermission = "허용"
        case .notDetermined: cameraPermission = "선택 전"
        default: cameraPermission = "허용 안 함"
        }
    }
    func stopSharing() async {
        guard let room, !busy else { return }
        busy = true
        defer { busy = false }
        do {
            try await session.setSharing(false, roomID: room.id)
            error = nil
        } catch { self.error = session.describe(error) }
    }
    func deleteNote() async {
        guard let room, !busy else { return }
        busy = true
        defer { busy = false }
        do {
            let token = try await session.repositories.auth.accessToken()
            try await session.repositories.notes.deleteNote(roomID: room.id, token: token)
            try await session.refresh()
            error = nil
        } catch { self.error = session.describe(error) }
    }
    var supportURL: URL? {
        guard let email = LegalConfiguration.supportEmail else { return nil }
        var components = URLComponents()
        components.scheme = "mailto"
        components.path = email
        components.queryItems = [URLQueryItem(name: "subject", value: "[곰만나] 문의"),
            URLQueryItem(name: "body", value: "문의 내용:\n\n\n앱 버전: \(version)\n\n비밀번호나 인증 토큰은 첨부하지 마세요.")]
        return components.url
    }
}
