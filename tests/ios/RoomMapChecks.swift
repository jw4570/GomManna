import Foundation
import CoreLocation
import MapKit

enum RoomMapChecks {
    @MainActor static func run() {
        var room = MockRoomData.room
        var first = room.members[0]
        first.isSharing = true
        first.latitude = 37.5; first.longitude = 127
        first.locationStatus = .fresh
        var second = room.members[1]
        second.isSharing = true
        second.latitude = 37.501; second.longitude = 127
        second.locationStatus = .fresh
        room.members = [first, second]
        room.myMemberID = first.id
        let suite = "GomManna.map-check.\(UUID())"
        let defaults = UserDefaults(suiteName: suite)!
        defer { defaults.removePersistentDomain(forName: suite) }
        let session = AppSession(repositories: .mock(service: MockAPIService()), defaults: defaults)
        session.enter(RoomSession(room: room, hostUserID: first.userID, myUserID: first.userID,
            sharingEpoch: 0, status: "active", createdAtMs: 0, endedAtMs: nil))
        let map = RoomViewModel(members: room.members, session: session)
        map.resetEntryCamera()
        precondition(map.camera.region?.center.latitude == first.latitude)
        var sharingOff = first
        sharingOff.isSharing = false
        map.updateMembers([sharingOff, second])
        map.resetEntryCamera()
        precondition(map.camera.region?.center.latitude == 37.5665)
        map.updateMembers(room.members)
        map.resetEntryCamera()
        precondition(map.camera.region?.center.latitude == first.latitude)
        precondition(map.distanceText(to: second) == "111 m")
        precondition(map.distanceText(to: first) == "0 m")

        let picker = MeetingPointViewModel(members: room.members)
        precondition(picker.selectedIDs.count == 2)
        for method in MeetingPointViewModel.Method.allCases {
            picker.method = method
            guard let point = picker.calculate(members: room.members) else { fatalError("Midpoint calculation failed") }
            precondition(abs(point.latitude - 37.5005) < 0.00001)
            precondition(abs(point.longitude - 127) < 0.00001)
        }
        picker.toggle(second)
        let solo = picker.calculate(members: room.members)!
        precondition(abs(solo.latitude - first.latitude!) < 0.00001)
        picker.toggle(first)
        precondition(picker.calculate(members: room.members) == nil && picker.error != nil)
        picker.toggle(first); picker.toggle(second)
        second.isSharing = false
        picker.reconcile(members: [first, second])
        precondition(picker.selectedIDs == [first.id])
        precondition(map.distanceLabel(to: second) == "위치 공유 꺼짐")

        map.setMeetingPointEnabled(true)
        precondition(map.meetingPoint.coordinate != nil && map.marks.isEmpty)
        let original = map.meetingPoint.coordinate!
        map.toggleMarking()
        map.pinMeetingPoint()
        precondition(map.pendingMark?.coordinate.latitude == original.latitude)
        let pendingID = map.pendingMark!.id
        precondition(map.displayedMarks.count == 1 && map.displayedMarks[0].id == pendingID)
        map.saveMark()
        precondition(map.marks.count == 1)
        precondition(!map.meetingPoint.isEnabled && map.meetingPoint.coordinate == nil)
        let id = map.marks[0].id
        precondition(id == pendingID && map.displayedMarks.count == 1)
        precondition(map.displayedMarks[0].coordinate.latitude == original.latitude)
        second.isSharing = true
        second.latitude = 37.503
        map.setMeetingPointEnabled(true)
        map.updateMembers([first, second])
        precondition(map.meetingPoint.coordinate!.latitude > original.latitude)
        precondition(map.marks[0].coordinate.latitude == original.latitude)
        map.setMeetingPointEnabled(false)
        precondition(map.meetingPoint.coordinate == nil && map.marks.count == 1)
        map.clearMapSelection()
        map.tapMark(map.marks[0])
        precondition(map.selectedMark == id && !map.showingMarkName)
        map.tapMark(map.marks[0])
        precondition(map.showingMarkName)
        precondition(map.displayedMarks.count == 1)
        map.markName = "  정문  "
        map.saveMark()
        precondition(map.marks.count == 1 && map.marks[0].id == id && map.marks[0].name == "정문")
        map.tapMark(map.marks[0]); map.markName = "취소할 이름"; map.cancelMark()
        precondition(map.marks[0].name == "정문")
        map.tapMark(map.marks[0])
        precondition(map.canDeleteMark)
        map.deletePendingMark()
        precondition(map.marks.isEmpty && map.displayedMarks.isEmpty)
        precondition(map.selectedMark == nil && map.pendingMark == nil && !map.showingMarkName)
        let place = MKMapItem(placemark: MKPlacemark(coordinate: original))
        place.name = "테스트 건물"
        map.searchedPlace = place
        map.pinSearchedPlace()
        precondition(map.pendingMark == nil)
        map.toggleMarking()
        map.pinSearchedPlace()
        precondition(map.markName == "테스트 건물")
        map.cancelMark()
        precondition(map.searchedPlace != nil && map.marks.isEmpty)
        map.toggleMarking()
        map.pinSearchedPlace()
        map.saveMark()
        precondition(map.searchedPlace == nil && map.marks.count == 1)
        precondition(map.marks[0].coordinate.latitude == original.latitude)
        let otherPlace = MKMapItem(placemark: MKPlacemark(coordinate: solo))
        otherPlace.name = "두 번째 건물"
        map.searchedPlaces = [place, otherPlace]
        map.toggleMarking()
        map.pinSearchedPlace(otherPlace)
        precondition(map.markName == "두 번째 건물")
        precondition(map.pendingMark?.latitude == solo.latitude)
        map.saveMark()
        precondition(map.searchedPlaces.isEmpty && map.marks.count == 2)
        map.clearMapSelection()
        precondition(map.selectedMark == nil)
        map.updateMembers([first, second])
        first.isSharing = false
        map.updateMembers([first, second])
        precondition(map.distanceText(to: solo) == nil)
        session.clearRoom()
        map.setMeetingPointEnabled(true)
        map.updateMembers([])
        precondition(map.meetingPoint.coordinate == nil)
        print("PASS: C distance, midpoint ON/OFF and live movement, stable marker identity, rename/cancel/delete, missing locations")
    }
}
