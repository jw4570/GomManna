import Foundation

struct MemberLocation {
    let latitude: Double
    let longitude: Double
    let accuracyMeters: Double?
    let capturedAtMs: Int64
    let receivedAtMs: Int64
    let sharingEpoch: Int64
    let sequence: Int64
}
