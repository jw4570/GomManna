import Foundation

struct EmptyData: Codable {}
struct CreateRoomRequest: Encodable { let title: String; let nickname: String }
struct JoinRoomRequest: Encodable {
    let inviteCode: String
    let nickname: String
    enum CodingKeys: String, CodingKey { case nickname; case inviteCode = "invite_code" }
}
struct SharingRequest: Encodable { let enabled: Bool }
struct LocationRequest: Encodable {
    let latitude: Double
    let longitude: Double
    let accuracyM: Double?
    let capturedAtMs: Int64
    let sharingEpoch: Int64
    let sequence: Int64

    enum CodingKeys: String, CodingKey {
        case latitude, longitude, sequence
        case accuracyM = "accuracy_m"
        case capturedAtMs = "captured_at_ms"
        case sharingEpoch = "sharing_epoch"
    }
}
struct NoteRequest: Encodable {
    let text: String
    let sharingEpoch: Int64
    enum CodingKeys: String, CodingKey { case text; case sharingEpoch = "sharing_epoch" }
}
struct MemberData: Decodable { let member: MemberDTO }
struct LocationData: Decodable { let location: LocationDTO }
struct NoteData: Decodable { let note: NoteDTO }
struct HealthDTO: Decodable { let status: String }
