import Foundation

struct MemberNote: Hashable {
    struct Origin: Hashable {
        let latitude: Double
        let longitude: Double
        let capturedAtMs: Int64
    }

    let id: UUID
    let text: String
    let writtenAtMs: Int64
    let origin: Origin?
    let sharingEpoch: Int64
    let photoAvailable: Bool
    let photoUploadedAtMs: Int64?
    let photoPath: String?
}
