import Foundation

struct RoomDTO: Decodable {
    let roomID: UUID
    let title: String
    let hostID: UUID
    let inviteCode: String
    let status: String
    let createdAtMs: Int64
    let endedAtMs: Int64?

    enum CodingKeys: String, CodingKey {
        case title, status
        case roomID = "room_id"
        case hostID = "host_id"
        case inviteCode = "invite_code"
        case createdAtMs = "created_at_ms"
        case endedAtMs = "ended_at_ms"
    }
}

struct RoomData: Decodable { let room: RoomDTO }
struct RoomMembershipData: Decodable {
    let room: RoomDTO
    let member: MemberDTO
}
struct InviteDTO: Decodable {
    let inviteCode: String
    let qrPayload: String
    let qrSVG: String

    enum CodingKeys: String, CodingKey {
        case inviteCode = "invite_code"
        case qrPayload = "qr_payload"
        case qrSVG = "qr_svg"
    }
}
