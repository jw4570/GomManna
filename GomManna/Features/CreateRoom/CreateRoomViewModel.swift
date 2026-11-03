import Foundation
import Observation

@Observable final class CreateRoomViewModel {
    var title = ""
    var name = ""
    var canCreate: Bool { !title.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty && !name.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty }
    func makeRoom() -> Room {
        let nickname = name.trimmingCharacters(in: .whitespacesAndNewlines)
        let me = RoomMember(id: UUID(), userID: UUID(), name: nickname, character: ProfileViewModel.shared.character, isHost: true, isSharing: false, latitude: nil, longitude: nil)
        return Room(title: title.trimmingCharacters(in: .whitespacesAndNewlines), code: String(UUID().uuidString.replacingOccurrences(of: "-", with: "").prefix(6)), myName: nickname, isHost: true, members: [me], myMemberID: me.id)
    }
}
