import Foundation

/// 메모나 사진이 바뀌면 이전 이미지를 다시 표시하지 않도록 요청을 구분합니다.
struct MemberPhotoRequest: Hashable {
    let roomID: UUID
    let memberID: UUID
    let noteID: UUID
    let writtenAtMs: Int64
    let sharingEpoch: Int64
    let photoUploadedAtMs: Int64?
    let photoPath: String?

    init?(roomID: UUID, member: RoomMember) {
        guard member.isSharing, let note = member.note, note.photoAvailable, note.sharingEpoch == member.sharingEpoch else { return nil }
        self.init(roomID: roomID, memberID: member.id, note: note)
    }

    init(roomID: UUID, memberID: UUID, note: MemberNote) {
        self.roomID = roomID
        self.memberID = memberID
        noteID = note.id
        writtenAtMs = note.writtenAtMs
        sharingEpoch = note.sharingEpoch
        photoUploadedAtMs = note.photoUploadedAtMs
        photoPath = note.photoPath
    }
}
