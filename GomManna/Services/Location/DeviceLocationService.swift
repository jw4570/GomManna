import CoreLocation
import Observation
#if canImport(UIKit)
import UIKit
#endif

@MainActor
protocol LocationProviding: AnyObject {
    var onLocation: ((CLLocation) -> Void)? { get set }
    var message: String? { get }
    var isRunning: Bool { get }
    func start()
    func stop()
}

@MainActor
@Observable final class DeviceLocationService: NSObject, CLLocationManagerDelegate, LocationProviding {
    private let manager = CLLocationManager()
    private(set) var latest: CLLocation?
    private(set) var message: String?
    private(set) var isRunning = false
    var onLocation: ((CLLocation) -> Void)?
    private var wanted = false

    override init() {
        super.init()
        manager.delegate = self
        manager.desiredAccuracy = kCLLocationAccuracyNearestTenMeters
        manager.distanceFilter = kCLDistanceFilterNone
        #if os(iOS)
        let modes = Bundle.main.object(forInfoDictionaryKey: "UIBackgroundModes") as? [String] ?? []
        manager.allowsBackgroundLocationUpdates = modes.contains("location")
        manager.showsBackgroundLocationIndicator = true
        manager.pausesLocationUpdatesAutomatically = false
        manager.activityType = .otherNavigation
        #endif
    }

    func start() {
        wanted = true
        guard !isRunning else { return }
        switch manager.authorizationStatus {
        case .notDetermined:
            #if os(iOS)
            guard UIApplication.shared.applicationState == .active else { return }
            #endif
            manager.requestWhenInUseAuthorization()
        case .authorizedAlways, .authorizedWhenInUse:
            #if os(iOS)
            // When In Use의 연속 백그라운드 업데이트는 포그라운드에서 시작합니다.
            guard UIApplication.shared.applicationState == .active else {
                message = "앱을 열면 위치 공유가 재개돼요."
                return
            }
            #endif
            manager.startUpdatingLocation()
            isRunning = true
            message = nil
        default:
            manager.stopUpdatingLocation()
            isRunning = false
            latest = nil
            message = "위치 권한이 꺼져 있어요. 설정에서 허용해 주세요."
        }
    }
    func stop() {
        wanted = false
        isRunning = false
        manager.stopUpdatingLocation()
        latest = nil
    }
    func locationManagerDidChangeAuthorization(_ manager: CLLocationManager) {
        switch manager.authorizationStatus {
        case .authorizedAlways, .authorizedWhenInUse: break
        default:
            isRunning = false
            manager.stopUpdatingLocation()
            latest = nil
        }
        if wanted { start() }
    }
    func locationManager(_ manager: CLLocationManager, didUpdateLocations locations: [CLLocation]) {
        guard wanted, isRunning, let location = locations.last, location.horizontalAccuracy >= 0 else { return }
        latest = location
        message = nil
        onLocation?(location)
    }
    func locationManager(_ manager: CLLocationManager, didFailWithError error: Error) {
        if (error as? CLError)?.code != .locationUnknown { message = error.localizedDescription }
    }
}

/// macOS CLI 검증에서는 위치 권한을 요청하지 않습니다. 테스트는 별도 공급자를 주입합니다.
@MainActor
final class InactiveLocationProvider: LocationProviding {
    var onLocation: ((CLLocation) -> Void)?
    var message: String? { nil }
    var isRunning: Bool { false }
    func start() {}
    func stop() {}
}
