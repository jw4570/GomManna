import Foundation

enum LocationStatus: String, Decodable {
    case off
    case noData = "no_data"
    case fresh
    case delayed
    case stale
}

struct MemberDTO: Decodable, Identifiable {
    let memberID: UUID
    let roomID: UUID
    let userID: UUID
    let nickname: String
    let isSharing: Bool
    let sharingEpoch: Int64
    let joinedAtMs: Int64
    
    let locationStatus: LocationStatus?
    let location: LocationDTO?
    let note: NoteDTO?
    
    var id: UUID { memberID }
    
    enum CodingKeys: String, CodingKey {
        case nickname, location, note
        case memberID = "member_id"
        case roomID = "room_id"
        case userID = "user_id"
        case isSharing = "is_sharing"
        case sharingEpoch = "sharing_epoch"
        case joinedAtMs = "joined_at_ms"
        case locationStatus = "location_status"
    }
}

struct MembersData: Decodable {
    let members: [MemberDTO]
}
