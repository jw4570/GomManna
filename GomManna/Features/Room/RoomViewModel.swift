import SwiftUI
import MapKit
import Observation
import OSLog

@Observable final class RoomViewModel {
    private static let responseLogger = Logger(subsystem: "GomManna", category: "MapResponse")
    var expanded = false
    var panelDragTranslation: CGFloat = 0
    var showingNotes = false
    var showingSettings = false
    var showingProfile = false
    var selectedMember: UUID?
    var selectedMark: UUID?
    var showingMeetingPoint = false
    let meetingPoint: MeetingPointViewModel
    var expandedNoteMember: UUID?
    let noteZoom = MapNoteZoomState()
    private(set) var expandedPhotoMember: UUID?
    var isMarking = false
    private(set) var marks: [MapMark] = []
    private(set) var pendingMark: MapMark?
    var searchedPlaces: [MKMapItem] = []
    var searchedPlace: MKMapItem? {
        get { searchedPlaces.first }
        set { searchedPlaces = newValue.map { [$0] } ?? [] }
    }
    private enum MarkSource { case meetingPoint, search }
    private var pendingMarkSource: MarkSource?
    var displayedMarks: [MapMark] {
        guard let pendingMark, !marks.contains(where: { $0.id == pendingMark.id }) else { return marks }
        return marks + [pendingMark]
    }
    var markName = ""
    var showingMarkName = false
    var canSaveMark: Bool { !markName.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty }
    var canDeleteMark: Bool {
        guard let pendingMark else { return false }
        return marks.contains { $0.id == pendingMark.id }
    }
    var members: [RoomMember] = []
    var errorMessage: String?

    let session: AppSession
    var locationMessage: String? { session.locationSharing.message ?? session.locationSharing.device.message }
    var changingSharing = false

    init(members: [RoomMember] = [], session: AppSession? = nil) {
        self.members = members
        self.meetingPoint = MeetingPointViewModel(members: members)
        self.session = session ?? .shared
    }
    var me: RoomMember? { members.first { $0.id == session.current?.room.myMemberID } }
    var isSharing: Bool { me?.isSharing ?? false }

    func resetEntryCamera() {
        var center = CLLocationCoordinate2D(latitude: 37.5665, longitude: 126.9780)
        if let me, me.isSharing {
            if let device = session.locationSharing.device as? DeviceLocationService,
               let location = device.latest,
               location.horizontalAccuracy >= 0,
               abs(location.timestamp.timeIntervalSinceNow) < 60,
               CLLocationCoordinate2DIsValid(location.coordinate) {
                center = location.coordinate
            } else if let coordinate = coordinate(for: me) {
                center = coordinate
            }
        }
        var transaction = Transaction(animation: nil)
        transaction.disablesAnimations = true
        withTransaction(transaction) {
            camera = .region(MKCoordinateRegion(center: center,
                span: MKCoordinateSpan(latitudeDelta: 0.012, longitudeDelta: 0.012)))
        }
    }

    func run(room: Room) async {
        while !Task.isCancelled, session.current?.room.id == room.id {
            do {
                try await session.refresh()
                guard let current = session.current, current.room.id == room.id else { return }
                updateMembers(current.room.members)
                errorMessage = nil
                try await Task.sleep(for: .seconds(5))
            } catch is CancellationError { return }
            catch {
                if Task.isCancelled || error.isRequestCancellation { return }
                errorMessage = session.describe(error)
                if session.recoveryNeeded { return }
                do { try await Task.sleep(for: .seconds(10)) } catch { return }
            }
        }
    }

    func setSharing(_ enabled: Bool, room: Room) async {
        guard !changingSharing else { return }
        changingSharing = true
        defer { changingSharing = false }
        do {
            try await session.setSharing(enabled, roomID: room.id)
            updateMembers(session.current?.room.members ?? [])
            errorMessage = nil
        } catch { errorMessage = session.describe(error) }
    }

    func distanceText(to member: RoomMember) -> String? {
        guard let end = coordinate(for: member) else { return nil }
        return distanceText(to: end)
    }

    func distanceText(to coordinate: CLLocationCoordinate2D) -> String? {
        guard let me, let start = self.coordinate(for: me),
              let distance = try? DistanceService().calculate(from: start, to: coordinate) else { return nil }
        return distance < 1000 ? "\(Int(distance.rounded())) m" : String(format: "%.1f km", distance / 1000)
    }

    func distanceLabel(to member: RoomMember) -> String {
        guard member.isSharing else { return "위치 공유 꺼짐" }
        guard let distance = distanceText(to: member) else { return "거리 확인 중" }
        let previous = member.locationStatus == .stale || member.locationStatus == .delayed ||
            me?.locationStatus == .stale || me?.locationStatus == .delayed
        return previous ? "약 \(distance) · 이전 위치" : distance
    }

    func distanceLabel(to mark: MapMark) -> String {
        guard let distance = distanceText(to: mark.coordinate) else { return "내 위치 확인 필요" }
        let previous = me?.locationStatus == .stale || me?.locationStatus == .delayed
        return previous ? "약 \(distance) · 이전 위치" : distance
    }

    func coordinate(for member: RoomMember) -> CLLocationCoordinate2D? {
        guard member.isSharing,
              let latitude = member.latitude, let longitude = member.longitude,
              latitude.isFinite, longitude.isFinite else { return nil }
        let coordinate = CLLocationCoordinate2D(latitude: latitude, longitude: longitude)
        return CLLocationCoordinate2DIsValid(coordinate) ? coordinate : nil
    }
    
    func toggleMarking() {
        clearMapSelection()
        isMarking.toggle()
    }

    func placeMark(at coordinate: CLLocationCoordinate2D) {
        guard isMarking else { return }
        pendingMarkSource = nil
        pendingMark = MapMark(name: "이름 설정", latitude: coordinate.latitude, longitude: coordinate.longitude)
        markName = ""
        isMarking = false
        showingMarkName = true
    }

    func saveMark() {
        guard var mark = pendingMark, canSaveMark else { return }
        mark.name = markName.trimmingCharacters(in: .whitespacesAndNewlines)
        if let index = marks.firstIndex(where: { $0.id == mark.id }) { marks[index] = mark }
        else { marks.append(mark) }
        selectedMark = mark.id
        switch pendingMarkSource {
        case .meetingPoint: meetingPoint.setEnabled(false, members: members)
        case .search: searchedPlace = nil
        case nil: break
        }
        cancelMark()
    }

    func tapMark(_ mark: MapMark) {
        guard !isMarking else { return }
        if selectedMark == mark.id {
            pendingMark = mark
            markName = mark.name
            showingMarkName = true
        } else {
            clearMapSelection()
            selectedMark = mark.id
        }
    }

    func setMeetingPointEnabled(_ enabled: Bool) {
        meetingPoint.setEnabled(enabled, members: members)
        guard enabled, let coordinate = meetingPoint.coordinate else { return }
        withAnimation(.easeOut(duration: 0.18)) {
            camera = .region(MKCoordinateRegion(center: coordinate,
                span: MKCoordinateSpan(latitudeDelta: 0.012, longitudeDelta: 0.012)))
        }
    }

    func pinMeetingPoint() {
        guard isMarking, let coordinate = meetingPoint.coordinate else { return }
        placeMark(at: coordinate)
        pendingMarkSource = .meetingPoint
        markName = "중간 지점 · \(meetingPoint.method.title)"
    }

    func pinSearchedPlace(_ selectedPlace: MKMapItem? = nil) {
        guard isMarking, let place = selectedPlace ?? searchedPlace else { return }
        placeMark(at: place.placemark.coordinate)
        pendingMarkSource = .search
        markName = place.name ?? "검색한 장소"
    }

    func cancelMark() {
        pendingMarkSource = nil
        pendingMark = nil
        markName = ""
        showingMarkName = false
    }

    func deletePendingMark() {
        guard canDeleteMark, let id = pendingMark?.id else { return }
        marks.removeAll { $0.id == id }
        if selectedMark == id { selectedMark = nil }
        cancelMark()
    }

    var camera: MapCameraPosition = .region(MKCoordinateRegion(center: CLLocationCoordinate2D(latitude: 37.5665, longitude: 126.9780), span: MKCoordinateSpan(latitudeDelta: 0.012, longitudeDelta: 0.012)))
    /// 짧게 밀어도 끝 위치로 이동하고, 작은 손 떨림은 기존 상태로 돌아갑니다.
    func finishPanelDrag(translation: CGFloat, predictedTranslation: CGFloat) {
        let momentum = predictedTranslation - translation
        if abs(momentum) >= 44 {
            expanded = momentum < 0
        } else if abs(translation) >= 24 {
            expanded = translation < 0
        }
        panelDragTranslation = 0
    }

    func updateMembers(_ updated: [RoomMember]) {
        members = updated
        meetingPoint.refresh(members: updated)
        if let id = expandedPhotoMember,
           !updated.contains(where: { $0.id == id && $0.isSharing && $0.note?.photoAvailable == true }) {
            expandedPhotoMember = nil
        }
    }

    func clearMapSelection() {
        var transaction = Transaction(animation: nil)
        transaction.disablesAnimations = true
        withTransaction(transaction) {
            selectedMark = nil
            selectedMember = nil
            expandedPhotoMember = nil
            expandedNoteMember = nil
        }
        Self.responseLogger.debug("Map selection cleared locally")
    }

    func tapMapNote(_ member: RoomMember) {
        expandedNoteMember = expandedNoteMember == member.id ? nil : member.id
    }

    func updateMapZoom(distance: CLLocationDistance) {
        noteZoom.update(distance: distance)
    }

    func tapMapProfile(_ member: RoomMember) {
        focus(member)
        if member.isSharing, member.note?.photoAvailable == true {
            expandedPhotoMember = expandedPhotoMember == member.id ? nil : member.id
        } else { expandedPhotoMember = nil }
        Self.responseLogger.debug("Map profile selected locally; photo request runs separately")
    }

    func focus(_ member: RoomMember) {
        guard let coordinate = coordinate(for: member) else { return }
        if expandedPhotoMember != member.id { expandedPhotoMember = nil }
        selectedMark = nil
        selectedMember = member.id
        expandedNoteMember = member.id
        withAnimation(.easeOut(duration: 0.18)) {
            camera = .region(MKCoordinateRegion(center: coordinate, span: MKCoordinateSpan(latitudeDelta: 0.006, longitudeDelta: 0.006)))
        }
    }
}
