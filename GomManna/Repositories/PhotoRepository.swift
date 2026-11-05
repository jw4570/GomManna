import Foundation

struct PhotoRepository {
    enum Format { case jpeg, png }

    private let service: any PhotoServiceProtocol
    init(service: any PhotoServiceProtocol) { self.service = service }

    /// 방향을 보정한 이미지 바이트를 전달합니다. 메모 저장 응답을 사용하므로 작성 시각·epoch가 일치합니다.
    func uploadPhoto(roomID: UUID, note: MemberNote, imageData: Data,
                     format: Format, token: String) async throws -> MemberNote {
        let updated = try await service.uploadPhoto(roomID: roomID, noteID: note.id,
                                                  writtenAtMs: note.writtenAtMs, sharingEpoch: note.sharingEpoch,
                                                  imageData: imageData, format: format == .jpeg ? .jpeg : .png,
                                                  token: token)
        return MemberNote(dto: updated)
    }

    /// 공유 OFF 이후 사진이 캐시에 남아 노출되지 않도록 여기서는 캐시하지 않습니다.
    func fetchPhoto(roomID: UUID, memberID: UUID, token: String) async throws -> Data {
        try await service.fetchPhoto(roomID: roomID, memberID: memberID, token: token)
    }
}
