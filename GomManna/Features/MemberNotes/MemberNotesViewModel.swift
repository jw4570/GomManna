import Foundation
import Observation

@MainActor
@Observable final class MemberNotesViewModel {
    let session: AppSession
    var noteDraft = ""
    var savingNote = false
    var errorMessage: String?
    var members: [RoomMember] { session.current?.room.members ?? [] }
    var me: RoomMember? { members.first { $0.id == session.current?.room.myMemberID } }
    var isSharing: Bool { me?.isSharing ?? false }
    init(session: AppSession) {
        self.session = session
        noteDraft = me?.note?.text ?? ""
    }

    func saveNote(room: Room) async {
        guard !savingNote, let me, me.isSharing else { return }
        savingNote = true
        defer { savingNote = false }
        do {
            let token = try await session.repositories.auth.accessToken()
            _ = try await session.repositories.notes.updateNote(roomID: room.id,
                text: noteDraft, sharingEpoch: me.sharingEpoch, token: token)
            try await session.refresh()
            errorMessage = nil
        } catch { errorMessage = session.describe(error) }
    }

    func deleteNote(room: Room) async {
        guard !savingNote else { return }
        savingNote = true
        defer { savingNote = false }
        do {
            let token = try await session.repositories.auth.accessToken()
            try await session.repositories.notes.deleteNote(roomID: room.id, token: token)
            noteDraft = ""
            try await session.refresh()
            errorMessage = nil
        } catch { errorMessage = session.describe(error) }
    }

    func uploadPhoto(_ data: Data, room: Room) async {
        guard !savingNote, let note = me?.note else { return }
        savingNote = true
        defer { savingNote = false }
        do {
            let normalized = try PhotoPreparationService().jpeg(from: data)
            let token = try await session.repositories.auth.accessToken()
            _ = try await session.repositories.photos.uploadPhoto(roomID: room.id, note: note,
                imageData: normalized, format: .jpeg, token: token)
            try await session.refresh()
            errorMessage = nil
        } catch { errorMessage = session.describe(error) }
    }

}
