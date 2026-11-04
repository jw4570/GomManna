import Foundation

struct RoomMember: Identifiable, Hashable {
    var id: UUID
    var userID: UUID
    var name: String
    var character: AnimalCharacter
    var isHost: Bool
    var isSharing: Bool
    
    var latitude: Double?
    var longitude: Double?
}

struct Room: Identifiable, Hashable {
    var id = UUID()
    var title: String
    var code: String
    var myName: String
    var isHost: Bool
    var members: [RoomMember]
    var myMemberID: UUID

    static var sample: Room { MockRoomData.room }
}
