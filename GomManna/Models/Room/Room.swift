import Foundation

struct RoomMember: Identifiable, Hashable {
    var id = UUID()
    var name: String
    var character: AnimalCharacter = .random()
    var isHost = false
    var latitude: Double = 37.5665
    var longitude: Double = 126.9780
}

struct Room: Identifiable, Hashable {
    var id = UUID()
    var title: String
    var code: String
    var myName: String
    var isHost: Bool
    var members: [RoomMember]
    var myMemberID: UUID

    static let sample: Room = {
        let me = RoomMember(name: "나", isHost: true)
        return Room(title: "오늘, 서울 산책", code: "SEOUL6", myName: "나", isHost: true, members: [
            me,
            RoomMember(name: "주원", latitude: 37.5680, longitude: 126.9760),
            RoomMember(name: "서영", latitude: 37.5650, longitude: 126.9800)
        ], myMemberID: me.id)
    }()
}
