import Foundation

enum MapNotePhotoChecks {
    static func run() {
        let roomID = UUID()
        let memberID = UUID()
        let noteID = UUID()
        func note(id: UUID? = nil, photo: Bool = true, uploaded: Int64 = 10, epoch: Int64 = 1) -> MemberNote {
            MemberNote(id: id ?? noteID, text: "입구에서 기다릴게요", writtenAtMs: 1, origin: nil,
                       sharingEpoch: epoch, photoAvailable: photo, photoUploadedAtMs: uploaded, photoPath: "/photo")
        }
        var member = RoomMember(id: memberID, userID: UUID(), name: "테스트", character: .bear,
            isHost: false, isSharing: true, latitude: 37.5, longitude: 127, sharingEpoch: 1, note: note())
        let original = MemberPhotoRequest(roomID: roomID, member: member)
        precondition(original != nil)
        member.latitude = 37.6
        precondition(MemberPhotoRequest(roomID: roomID, member: member) == original, "GPS 갱신으로 사진을 다시 다운로드하지 않아야 합니다")
        member.note = note(uploaded: 20)
        precondition(MemberPhotoRequest(roomID: roomID, member: member) != original)
        member.note = note(id: UUID())
        precondition(MemberPhotoRequest(roomID: roomID, member: member) != original)
        member.note = note(photo: false)
        precondition(MemberPhotoRequest(roomID: roomID, member: member) == nil)
        member.note = note(epoch: 2)
        precondition(MemberPhotoRequest(roomID: roomID, member: member) == nil)
        member.note = note()
        member.isSharing = false
        precondition(MemberPhotoRequest(roomID: roomID, member: member) == nil)
        member.note = nil
        precondition(MemberPhotoRequest(roomID: roomID, member: member) == nil)
        print("PASS: map photo identity, GPS reuse, photo/note invalidation, sharing OFF and epoch privacy")
    }
}
