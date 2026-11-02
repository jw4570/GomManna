import Foundation
import Observation

@Observable final class CreateRoomViewModel {
    var title = ""
    var name = ""
    var canCreate: Bool { !title.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty && !name.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty }
    func makeRoom() -> Room {
        let nickname = name.trimmingCharacters(in: .whitespacesAndNewlines)
        let me = RoomMember(name: nickname, character: ProfileViewModel.shared.character, isHost: true)
        return Room(title: title.trimmingCharacters(in: .whitespacesAndNewlines), code: String(UUID().uuidString.replacingOccurrences(of: "-", with: "").prefix(6)), myName: nickname, isHost: true, members: [me], myMemberID: me.id)
    }
}
