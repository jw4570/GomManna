import Foundation
import Observation

/// 주석에서만 관찰하여 확대 중 방 화면 전체를 다시 계산하지 않습니다.
@Observable final class MapNoteZoomState {
    private(set) var isExpanded = false

    func update(distance: Double) {
        guard distance.isFinite else { return }
        if distance <= 900, !isExpanded { isExpanded = true }
        else if distance >= 1_200, isExpanded { isExpanded = false }
    }
}
