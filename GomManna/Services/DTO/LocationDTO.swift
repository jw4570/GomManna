import Foundation

struct LocationDTO: Decodable {
    let latitude: Double
    let longitude: Double
    let accuracyM: Double?
    let capturedAtMs: Int64
    let receivedAtMs: Int64
    let sharingEpoch: Int64
    let sequence: Int64
    
    enum CodingKeys: String, CodingKey {
        case latitude, longitude, sequence
        case accuracyM = "accuracy_m"
        case capturedAtMs = "captured_at_ms"
        case receivedAtMs = "received_at_ms"
        case sharingEpoch = "sharing_epoch"
    }
}
