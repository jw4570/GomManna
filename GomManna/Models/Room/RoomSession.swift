import Foundation

/// 서버에서 발급한 ID와 공유 회차를 포함한 방 입장 결과입니다.
struct RoomSession {
    let room: Room
    let hostUserID: UUID
    let myUserID: UUID
    let sharingEpoch: Int64
    let status: String
    let createdAtMs: Int64
    let endedAtMs: Int64?
}

struct RoomInvitation {
    let code: String
    let qrPayload: String
    let qrSVG: String
}

struct SharingState {
    let memberID: UUID
    let isSharing: Bool
    let epoch: Int64
}
