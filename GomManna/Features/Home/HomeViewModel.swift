import Foundation
import Observation

/// UI 확인용 메모리 상태. 동시에 하나의 모임만 참여합니다.
@Observable final class HomeViewModel {
    private(set) var currentRoom: Room?
    var path: [UUID] = []
    var showingCreate = false
    var showingJoin = false
    var showingProfile = false
    var showingInvite = false

    init(currentRoom: Room? = nil) {
        self.currentRoom = currentRoom
    }

    func enter(_ room: Room) {
        showingCreate = false
        showingJoin = false
        // 이미 참여 중이면 다른 모임으로 덮어쓰지 않습니다.
        if currentRoom == nil { currentRoom = room }
        openCurrentRoom()
    }

    func openCurrentRoom() {
        guard let currentRoom else { return }
        path = [currentRoom.id]
    }

    func remove(_ room: Room) {
        guard currentRoom?.id == room.id else { return }
        showingInvite = false
        path.removeAll()
        currentRoom = nil
    }
}
