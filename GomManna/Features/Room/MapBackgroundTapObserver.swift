import SwiftUI
import MapKit

/// 지도 기본 제스처를 막지 않고, 주석 바깥의 탭만 관찰합니다.
struct MapBackgroundTapObserver: UIViewRepresentable {
    var isMarking: Bool
    var onTap: (CLLocationCoordinate2D) -> Void

    func makeUIView(context: Context) -> ProbeView {
        let view = ProbeView()
        view.isUserInteractionEnabled = false
        view.coordinator = context.coordinator
        return view
    }

    func updateUIView(_ view: ProbeView, context: Context) {
        context.coordinator.onTap = onTap
        context.coordinator.isMarking = isMarking
        view.installObserver()
    }

    func makeCoordinator() -> Coordinator { Coordinator(isMarking: isMarking, onTap: onTap) }

    static func dismantleUIView(_ view: ProbeView, coordinator: Coordinator) {
        coordinator.detach()
    }

    final class ProbeView: UIView {
        weak var coordinator: Coordinator?

        override func didMoveToWindow() {
            super.didMoveToWindow()
            installObserver()
        }

        func installObserver() {
            DispatchQueue.main.async { [weak self] in
                guard let self, self.window != nil, let coordinator = self.coordinator else { return }
                var ancestor = self.superview
                while let container = ancestor {
                    if let map = self.findMap(in: container) {
                        coordinator.attach(to: map)
                        return
                    }
                    ancestor = container.superview
                }
            }
        }

        private func findMap(in view: UIView) -> MKMapView? {
            if let map = view as? MKMapView { return map }
            for child in view.subviews {
                if let map = findMap(in: child) { return map }
            }
            return nil
        }
    }

    final class Coordinator: NSObject, UIGestureRecognizerDelegate {
        var isMarking: Bool
        var onTap: (CLLocationCoordinate2D) -> Void
        private weak var map: MKMapView?
        private lazy var touchObserver: TouchDownObserver = {
            let gesture = TouchDownObserver(target: nil, action: nil)
            gesture.cancelsTouchesInView = false
            gesture.delaysTouchesBegan = false
            gesture.delaysTouchesEnded = false
            gesture.delegate = self
            gesture.onTouchDown = { [weak self] touch in
                guard let self, !self.isMarking, let map = self.map else { return }
                self.onTap(map.convert(touch.location(in: map), toCoordinateFrom: map))
            }
            return gesture
        }()
        private lazy var tap: UITapGestureRecognizer = {
            let gesture = UITapGestureRecognizer(target: self, action: #selector(tapped(_:)))
            gesture.cancelsTouchesInView = false
            gesture.delaysTouchesBegan = false
            gesture.delaysTouchesEnded = false
            gesture.delegate = self
            return gesture
        }()

        init(isMarking: Bool, onTap: @escaping (CLLocationCoordinate2D) -> Void) {
            self.isMarking = isMarking
            self.onTap = onTap
        }

        func attach(to map: MKMapView) {
            guard self.map !== map else { return }
            detach()
            self.map = map
            map.addGestureRecognizer(tap)
            map.addGestureRecognizer(touchObserver)
        }

        func detach() {
            map?.removeGestureRecognizer(tap)
            map?.removeGestureRecognizer(touchObserver)
            map = nil
        }

        @objc private func tapped(_ gesture: UITapGestureRecognizer) {
            guard isMarking, gesture.state == .ended, let map else { return }
            onTap(map.convert(gesture.location(in: map), toCoordinateFrom: map))
        }

        func gestureRecognizer(_ gestureRecognizer: UIGestureRecognizer, shouldReceive touch: UITouch) -> Bool {
            if gestureRecognizer === tap, !isMarking { return false }
            if gestureRecognizer === touchObserver, isMarking { return false }
            var view = touch.view
            while let current = view, current !== map {
                // 프로필·메모·사진 안의 탭은 해당 컨트롤에서 처리합니다.
                if current is MKAnnotationView || current is UIControl { return false }
                view = current.superview
            }
            return true
        }

        func gestureRecognizer(_ gestureRecognizer: UIGestureRecognizer,
                               shouldRecognizeSimultaneouslyWith otherGestureRecognizer: UIGestureRecognizer) -> Bool {
            true
        }
    }

    /// 탭 성공/실패나 지도 더블 탭 판정을 기다리지 않고 최초 터치를 관찰합니다.
    /// 즉시 실패 상태로 끝내므로 드래그·핀치·더블 탭을 가로채지 않습니다.
    final class TouchDownObserver: UIGestureRecognizer {
        var onTouchDown: ((UITouch) -> Void)?

        override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent) {
            if let touch = touches.first { onTouchDown?(touch) }
            state = .failed
        }
    }
}
