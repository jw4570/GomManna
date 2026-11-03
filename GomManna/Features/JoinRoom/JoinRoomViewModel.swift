import Foundation
import Observation

@Observable final class JoinRoomViewModel {
    enum Method: String, CaseIterable { case code = "코드 입력", qr = "QR 스캔" }
    var method: Method = .code
    var code = ""
    var name = ""
    var resolvedRoom: Room?
    var error: String?
    var normalizedCode: String { code.trimmingCharacters(in: .whitespacesAndNewlines).uppercased() }
    var canEnter: Bool { resolvedRoom != nil && !name.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty }
    func findRoom(in rooms: [Room]) {
        let availableRooms = rooms + (AppEnvironment.dataSource == .mock ? [MockRoomData.room] : [])
        resolvedRoom = availableRooms.first { $0.code == normalizedCode }
        error = resolvedRoom == nil ? "코드를 다시 확인해 주세요." : nil
    }
    func joinedRoom() -> Room? {
        guard var room = resolvedRoom else { return nil }
        room.myName = name.trimmingCharacters(in: .whitespacesAndNewlines)
        room.isHost = false
        let me = RoomMember(id: UUID(), userID: UUID(), name: room.myName, character: ProfileViewModel.shared.character, isHost: false, isSharing: false, latitude: nil, longitude: nil)
        room.members.append(me)
        room.myMemberID = me.id
        return room
    }
}
