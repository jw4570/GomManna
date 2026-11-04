import Foundation

/// 고정 ID를 사용하므로 다시 조회해도 같은 멤버로 인식합니다.
enum MockRoomData {
    private static func id(_ value: Int) -> UUID {
        UUID(uuidString: String(format: "00000000-0000-0000-0000-%012d", value))!
    }

    static let room: Room = {
        let members = [
            RoomMember(id: id(1), userID: id(101), name: "나", character: .bear, isHost: true,
                       isSharing: true, latitude: 37.5665, longitude: 126.9780),
            RoomMember(id: id(2), userID: id(102), name: "주원", character: .rabbit, isHost: false,
                       isSharing: true, latitude: 37.5680, longitude: 126.9760),
            RoomMember(id: id(3), userID: id(103), name: "서영", character: .fox, isHost: false,
                       isSharing: true, latitude: 37.5650, longitude: 126.9800),
            RoomMember(id: id(4), userID: id(104), name: "민수", character: .panda, isHost: false,
                       isSharing: false, latitude: nil, longitude: nil),
            RoomMember(id: id(5), userID: id(105), name: "지우", character: .penguin, isHost: false,
                       isSharing: true, latitude: nil, longitude: nil)
        ]
        return Room(id: id(1000), title: "오늘, 서울 산책", code: "SEOUL6", myName: "나",
                    isHost: true, members: members, myMemberID: members[0].id)
    }()
}
