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
    var expandedNoteMember: UUID?
    let noteZoom = MapNoteZoomState()
    private(set) var expandedPhotoMember: UUID?
    var isMarking = false
    private(set) var marks: [MapMark] = []
    private(set) var pendingMark: MapMark?
    var markName = ""
    var showingMarkName = false
    var canSaveMark: Bool { !markName.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty }
    var members: [RoomMember] = []
    var errorMessage: String?

    let session: AppSession
    var locationMessage: String? { session.locationSharing.message ?? session.locationSharing.device.message }
    var changingSharing = false

    init(members: [RoomMember] = [], session: AppSession? = nil) {
        self.members = members
        self.session = session ?? .shared
    }
    var me: RoomMember? { members.first { $0.id == session.current?.room.myMemberID } }
    var isSharing: Bool { me?.isSharing ?? false }

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
        guard let me, me.id != member.id, me.locationStatus == .fresh, member.locationStatus == .fresh,
              let start = coordinate(for: me), let end = coordinate(for: member),
              let distance = try? DistanceService().calculate(from: start, to: end) else { return nil }
        return distance < 1000 ? "\(Int(distance))m" : String(format: "%.1fkm", distance / 1000)
    }

    func coordinate(for member: RoomMember) -> CLLocationCoordinate2D? {
        guard member.isSharing,
              let latitude = member.latitude, let longitude = member.longitude,
              latitude.isFinite, longitude.isFinite else { return nil }
        let coordinate = CLLocationCoordinate2D(latitude: latitude, longitude: longitude)
        return CLLocationCoordinate2DIsValid(coordinate) ? coordinate : nil
    }
    
    func toggleMarking() { isMarking.toggle() }

    func placeMark(at coordinate: CLLocationCoordinate2D) {
        guard isMarking else { return }
        pendingMark = MapMark(name: "이름 설정", latitude: coordinate.latitude, longitude: coordinate.longitude)
        markName = ""
        isMarking = false
        showingMarkName = true
    }

    func saveMark() {
        guard var mark = pendingMark, canSaveMark else { return }
        mark.name = markName.trimmingCharacters(in: .whitespacesAndNewlines)
        marks.append(mark)
        cancelMark()
    }

    func cancelMark() {
        pendingMark = nil
        markName = ""
        showingMarkName = false
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
        if let id = expandedPhotoMember,
           !updated.contains(where: { $0.id == id && $0.isSharing && $0.note?.photoAvailable == true }) {
            expandedPhotoMember = nil
        }
    }

    func clearMapSelection() {
        var transaction = Transaction(animation: nil)
        transaction.disablesAnimations = true
        withTransaction(transaction) {
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
        selectedMember = member.id
        expandedNoteMember = member.id
        withAnimation(.easeOut(duration: 0.18)) {
            camera = .region(MKCoordinateRegion(center: coordinate, span: MKCoordinateSpan(latitudeDelta: 0.006, longitudeDelta: 0.006)))
        }
    }
}
