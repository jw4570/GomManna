import Foundation

struct GuestDTO: Decodable {
    let userID: UUID
    let token: String
    let expiresAtMs: Int64

    enum CodingKeys: String, CodingKey {
        case token
        case userID = "user_id"
        case expiresAtMs = "expires_at_ms"
    }
}
