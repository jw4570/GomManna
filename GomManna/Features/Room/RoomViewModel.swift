import SwiftUI
import MapKit
import Observation

@Observable final class RoomViewModel {
    var expanded = false
    var panelDragTranslation: CGFloat = 0
    var showingChat = false
    let chat = ChatViewModel()
    var showingSettings = false
    var showingProfile = false
    var selectedMember: UUID?
    var isMarking = false
    private(set) var marks: [MapMark] = []
    private(set) var pendingMark: MapMark?
    var markName = ""
    var showingMarkName = false
    var canSaveMark: Bool { !markName.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty }

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

    func focus(_ member: RoomMember) {
        selectedMember = member.id
        withAnimation { camera = .region(MKCoordinateRegion(center: CLLocationCoordinate2D(latitude: member.latitude, longitude: member.longitude), span: MKCoordinateSpan(latitudeDelta: 0.006, longitudeDelta: 0.006))) }
    }
}
