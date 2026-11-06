import MapKit
import Observation

@Observable
final class PlaceDetailsViewModel: Identifiable {
    let id = UUID()
    let place: MKMapItem

    init(place: MKMapItem) { self.place = place }

    var name: String { place.name ?? "장소 정보" }
    var address: String? { place.placemark.title }
    var phone: String? { place.phoneNumber }
    var website: URL? { place.url }
    var phoneURL: URL? {
        guard let phone else { return nil }
        let number = phone.filter { $0.isNumber || $0 == "+" }
        return number.isEmpty ? nil : URL(string: "tel:\(number)")
    }

    func openInMaps() { place.openInMaps() }

    func showDirections() {
        place.openInMaps(launchOptions: [MKLaunchOptionsDirectionsModeKey: MKLaunchOptionsDirectionsModeWalking])
    }
}
