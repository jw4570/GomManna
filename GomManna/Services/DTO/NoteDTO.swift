import Foundation

struct NoteOriginDTO: Decodable {
    let latitude: Double
    let longitude: Double
    let capturedAtMs: Int64
    
    enum CodingKeys: String, CodingKey {
        case latitude, longitude
        case capturedAtMs = "captured_at_ms"
    }
}

struct NoteDTO: Decodable {
    let noteID: UUID
    let text: String
    let writtenAtMs: Int64
    let origin: NoteOriginDTO?
    let sharingEpoch: Int64
    let photoAvailable: Bool
    let photoUploadedAtMs: Int64?
    let photoPath: String?
    
    enum CodingKeys: String, CodingKey {
        case text, origin
        case noteID = "note_id"
        case writtenAtMs = "written_at_ms"
        case sharingEpoch = "sharing_epoch"
        case photoAvailable = "photo_available"
        case photoUploadedAtMs = "photo_uploaded_at_ms"
        case photoPath = "photo_path"
    }
}
