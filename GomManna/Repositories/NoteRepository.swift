import Foundation

struct NoteRepository {
    private let service: any NoteServiceProtocol
    init(service: any NoteServiceProtocol) { self.service = service }

    func updateNote(roomID: UUID, text: String, sharingEpoch: Int64, token: String) async throws -> MemberNote {
        let note = try await service.updateNote(roomID: roomID, text: text, sharingEpoch: sharingEpoch, token: token)
        return MemberNote(dto: note)
    }

    func deleteNote(roomID: UUID, token: String) async throws {
        try await service.deleteNote(roomID: roomID, token: token)
    }
}

extension MemberNote {
    init(dto: NoteDTO) {
        id = dto.noteID
        text = dto.text
        writtenAtMs = dto.writtenAtMs
        origin = dto.origin.map { Origin(latitude: $0.latitude, longitude: $0.longitude, capturedAtMs: $0.capturedAtMs) }
        sharingEpoch = dto.sharingEpoch
        photoAvailable = dto.photoAvailable
        photoUploadedAtMs = dto.photoUploadedAtMs
        photoPath = dto.photoPath
    }
}
